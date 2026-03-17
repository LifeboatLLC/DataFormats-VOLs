#include "bufr_helper.h"

/* ============================================================
 *  Small utilities
 *
 *  These helpers implement memory allocation, bounded string
 *  copying, and parsing of ecCodes key-name conventions used by
 *  the rest of the module.
 * ============================================================ */

/*
 * xmalloc
 *     malloc() wrapper that aborts on allocation failure.
 *
 * Rationale:
 *     The helper code treats allocation failure as fatal rather than
 *     propagating NULL through every internal path.
 */
void* xmalloc(size_t n)
{
    void* p = malloc(n);
    if (!p) { fprintf(stderr, "Out of memory\n"); exit(1); }
    return p;
}

/*
 * xrealloc
 *     realloc() wrapper that aborts on allocation failure.
 */
void* xrealloc(void* p, size_t n)
{
    void* q = realloc(p, n);
    if (!q) { fprintf(stderr, "Out of memory\n"); exit(1); }
    return q;
}

/*
 * bufr_safe_strcpy
 *     Copy src into dst with guaranteed NUL termination when the
 *     destination capacity is non-zero. A NULL source becomes an
 *     empty string.
 */
void bufr_safe_strcpy(char* dst, size_t cap, const char* src)
{
    if (!dst || cap == 0) return;
    if (!src) { dst[0] = '\0'; return; }
    strncpy(dst, src, cap - 1);
    dst[cap - 1] = '\0';
}

/*
 * ends_with
 *     Return non-zero when string s ends with suffix.
 */
static int ends_with(const char* s, const char* suffix)
{
    size_t ns = strlen(s), nf = strlen(suffix);
    if (nf > ns) return 0;
    return memcmp(s + (ns - nf), suffix, nf) == 0;
}

/*
 * split_key_arrow
 *     Split an ecCodes metadata key of the form "left->right" into
 *     its base key and attribute suffix. Returns 1 on success.
 */
static int split_key_arrow(const char* key,
                           char* left, size_t left_cap,
                           char* right, size_t right_cap)
{
    const char* p = strstr(key, "->");
    if (!p) return 0;

    size_t nleft  = (size_t)(p - key);
    size_t nright = strlen(p + 2);
    if (nleft == 0 || nright == 0) return 0;
    if (nleft >= left_cap || nright >= right_cap) return 0;

    memcpy(left, key, nleft);
    left[nleft] = '\0';
    memcpy(right, p + 2, nright);
    right[nright] = '\0';
    return 1;
}

/*
 * parse_replicated_key
 *     Parse a replicated BUFR key of the form "#n#base".
 *
 * Returns:
 *     1 when the key matches the replicated syntax and the parsed
 *     index/base are returned, otherwise 0.
 */
static int parse_replicated_key(const char* key,
                                int* out_n,
                                char* out_base,
                                size_t out_cap)
{
    if (!key || key[0] != '#') return 0;

    const char* p = key + 1;
    int n = 0;

    if (*p < '0' || *p > '9') return 0;
    while (*p >= '0' && *p <= '9') {
        n = (n * 10) + (*p - '0');
        p++;
    }
    if (*p != '#') return 0;

    p++;
    if (!*p) return 0;

    size_t len = strlen(p);
    if (len >= out_cap) len = out_cap - 1;
    memcpy(out_base, p, len);
    out_base[len] = '\0';

    if (out_n) *out_n = n;
    return 1;
}

/* ============================================================
 *  Mapping policy: which "->attr" keys become datasets "base_attr"
 * ============================================================ */

/* Only these suffixes are mapped to base_attr dataset names.
 * Add more if needed. */
static const char* const g_known_meta_attrs[] = {
    "percentConfidence",
    "units",
    "code",
    "scale",
    "reference",
    "width",
    NULL
};

/*
 * make_meta_dataset_name
 *     Convert a metadata key pair (base, attr) into the synthesized
 *     HDF5 dataset name "base_attr".
 */
static void make_meta_dataset_name(const char* base, const char* attr,
                                   char* out, size_t out_cap)
{
    snprintf(out, out_cap, "%s_%s", base, attr);
}

/*
 * inv_init
 *     Initialize an inventory structure to the empty state.
 */
static void inv_init(inv_t* inv) { memset(inv, 0, sizeof(*inv)); }

/*
 * bufr_inv_free
 *     Release dynamic arrays owned by the inventory and reset all
 *     fields to zero so the structure can be safely reused.
 */
void bufr_inv_free(inv_t* inv)
{
    if (!inv) return;
    for (size_t i = 0; i < inv->ndatasets; i++) {
        free(inv->datasets[i].attrs);
        inv->datasets[i].attrs = NULL;
        inv->datasets[i].nattrs = 0;
        inv->datasets[i].capattrs = 0;
    }
    free(inv->datasets);
    free(inv->group_attrs);
    memset(inv, 0, sizeof(*inv));
}

/*
 * bufr_inv_print
 *     Print the inventory in a compact diagnostic format useful for
 *     debugging the BUFR-to-HDF5 mapping policy.
 */
void bufr_inv_print(const inv_t *inv)
{
    if (!inv) {
        printf("Inventory: NULL\n");
        return;
    }

    printf("\n===== BUFR INVENTORY =====\n");

    /* --------------------------- */
    /* Group attributes            */
    /* --------------------------- */

    printf("\nGroup attributes (%zu):\n", inv->ngroup_attrs);

    for (size_t i = 0; i < inv->ngroup_attrs; i++) {
        const inv_attr_t *a = &inv->group_attrs[i];

        printf("  GATTR %-40s type=%d size=%zu\n",
               a->name,
               a->native_type,
               a->size);
    }

    /* --------------------------- */
    /* Datasets                    */
    /* --------------------------- */

    printf("\nDatasets (%zu):\n", inv->ndatasets);

    for (size_t i = 0; i < inv->ndatasets; i++) {

        const inv_dataset_t *d = &inv->datasets[i];

        if (d->is_replicated) {
            printf("  DSET  %-35s type=%d occ_size=%zu REPL rep_count=%d attrs=%zu\n",
                   d->name,
                   d->native_type,
                   d->occ_size,
                   d->rep_count,
                   d->nattrs);
        } else {
            printf("  DSET  %-35s type=%d occ_size=%zu attrs=%zu\n",
                   d->name,
                   d->native_type,
                   d->occ_size,
                   d->nattrs);
        }

        /* --------------------------- */
        /* Dataset attributes          */
        /* --------------------------- */

        for (size_t j = 0; j < d->nattrs; j++) {

            const inv_dset_attr_t *a = &d->attrs[j];

            if (a->per_occurrence) {

                printf("        @%-30s type=%d size=%zu PER_OCC rep=%d\n",
                       a->name,
                       a->native_type,
                       a->size,
                       a->rep_count);

            } else {

                printf("        @%-30s type=%d size=%zu\n",
                       a->name,
                       a->native_type,
                       a->size);
            }
        }
    }

    printf("\n===== END INVENTORY =====\n");
}

/*
 * inv_group_attr_exists
 *     Return non-zero when a group attribute with the given name is
 *     already present in the inventory.
 */
static int inv_group_attr_exists(const inv_t* inv, const char* name)
{
    for (size_t i = 0; i < inv->ngroup_attrs; i++)
        if (strcmp(inv->group_attrs[i].name, name) == 0)
            return 1;
    return 0;
}

/*
 * inv_add_group_attr
 *     Append a new group attribute to the inventory unless it already
 *     exists. The backing array grows geometrically.
 */
static void inv_add_group_attr(inv_t* inv, const char* name, int ntype, size_t sz)
{
    if (!inv || !name || !*name) return;
    if (inv_group_attr_exists(inv, name)) return;

    if (inv->ngroup_attrs == inv->capgroup_attrs) {
        inv->capgroup_attrs = (inv->capgroup_attrs == 0) ? 128 : inv->capgroup_attrs * 2;
        inv->group_attrs = (inv_attr_t*)xrealloc(inv->group_attrs,
                                                 inv->capgroup_attrs * sizeof(inv_attr_t));
    }

    inv_attr_t* a = &inv->group_attrs[inv->ngroup_attrs++];
    memset(a, 0, sizeof(*a));
    bufr_safe_strcpy(a->name, sizeof(a->name), name);
    a->native_type = ntype;
    a->size = sz;
}

/*
 * inv_find_dataset
 *     Look up a dataset by its normalized HDF5-visible name.
 */
static inv_dataset_t* inv_find_dataset(inv_t* inv, const char* name)
{
    for (size_t i = 0; i < inv->ndatasets; i++)
        if (strcmp(inv->datasets[i].name, name) == 0)
            return &inv->datasets[i];
    return NULL;
}

/*
 * inv_get_or_add_dataset
 *     Return an existing dataset entry or create a new one. If the
 *     dataset already exists, missing type/occurrence-size metadata is
 *     filled in opportunistically.
 */
static inv_dataset_t* inv_get_or_add_dataset(inv_t* inv,
                                             const char* name,
                                             int ntype,
                                             size_t occ_size)
{
    if (!inv || !name || !*name) return NULL;

    inv_dataset_t* d = inv_find_dataset(inv, name);
    if (d) {
        if (d->native_type == 0 && ntype != 0) d->native_type = ntype;
        if (d->occ_size == 0 && occ_size != 0) d->occ_size = occ_size;
        return d;
    }

    if (inv->ndatasets == inv->capdatasets) {
        inv->capdatasets = (inv->capdatasets == 0) ? 128 : inv->capdatasets * 2;
        inv->datasets = (inv_dataset_t*)xrealloc(inv->datasets,
                                                 inv->capdatasets * sizeof(inv_dataset_t));
    }

    d = &inv->datasets[inv->ndatasets++];
    memset(d, 0, sizeof(*d));
    bufr_safe_strcpy(d->name, sizeof(d->name), name);
    d->native_type = ntype;
    d->occ_size = occ_size;
    d->is_replicated = 0;
    d->rep_count = 0;
    d->attrs = NULL;
    d->nattrs = 0;
    d->capattrs = 0;
    return d;
}

/*
 * inv_mark_dataset_replicated
 *     Mark a dataset as replicated and keep the largest replication
 *     index observed so far.
 */
static void inv_mark_dataset_replicated(inv_dataset_t* d, int rep_n)
{
    if (!d) return;
    d->is_replicated = 1;
    if (rep_n > d->rep_count) d->rep_count = rep_n;
}

/*
 * inv_find_dset_attr
 *     Look up a dataset attribute by name.
 */
static inv_dset_attr_t* inv_find_dset_attr(inv_dataset_t* d, const char* name)
{
    if (!d || !name) return NULL;
    for (size_t i = 0; i < d->nattrs; i++)
        if (strcmp(d->attrs[i].name, name) == 0)
            return &d->attrs[i];
    return NULL;
}

/*
 * inv_get_or_add_dset_attr
 *     Return an existing dataset-attribute entry or append a new one.
 *     The first known type/size wins.
 */
static inv_dset_attr_t* inv_get_or_add_dset_attr(inv_dataset_t* d,
                                                 const char* name,
                                                 int ntype,
                                                 size_t sz)
{
    if (!d || !name || !*name) return NULL;

    inv_dset_attr_t* a = inv_find_dset_attr(d, name);
    if (a) {
        /* Keep first known type/size */
        if (a->native_type == 0 && ntype != 0) a->native_type = ntype;
        if (a->size == 0 && sz != 0) a->size = sz;
        return a;
    }

    if (d->nattrs == d->capattrs) {
        d->capattrs = (d->capattrs == 0) ? 16 : d->capattrs * 2;
        d->attrs = (inv_dset_attr_t*)xrealloc(d->attrs,
                                              d->capattrs * sizeof(inv_dset_attr_t));
    }

    a = &d->attrs[d->nattrs++];
    memset(a, 0, sizeof(*a));
    bufr_safe_strcpy(a->name, sizeof(a->name), name);
    a->native_type = ntype;
    a->size = sz;
    a->per_occurrence = 0;
    a->rep_count = 0;
    return a;
}

/*
 * inv_mark_attr_per_occurrence
 *     Record that a dataset attribute varies per replicated occurrence.
 */
static void inv_mark_attr_per_occurrence(inv_dset_attr_t* a, int rep_n)
{
    if (!a) return;
    a->per_occurrence = 1;
    if (rep_n > a->rep_count) a->rep_count = rep_n;
}

/* ============================================================
 *  Classification policy for inventory
 * ============================================================ */

/*
 * force_group_attr_key
 *     Return non-zero for known BUFR header/control keys that should
 *     always be exposed as HDF5 group attributes.
 */
static int force_group_attr_key(const char* key)
{
    static const char* const exact[] = {
        "edition","masterTableNumber","masterTablesVersionNumber","localTablesVersionNumber",
        "bufrHeaderCentre","bufrHeaderSubCentre","dataCategory","dataSubCategory",
        "internationalDataSubCategory","typicalYear","typicalMonth","typicalDay",
        "typicalHour","typicalMinute","typicalSecond","numberOfSubsets","compressedData",
        NULL
    };
    for (int i = 0; exact[i]; i++)
        if (strcmp(key, exact[i]) == 0) return 1;
    return 0;
}

/*
 * is_descriptor_key
 *     Return non-zero for descriptor arrays that should always be
 *     exposed as datasets.
 */
static int is_descriptor_key(const char* key)
{
    static const char* const exact[] = {
        "unexpandedDescriptors","expandedDescriptors","shortExpandedDescriptors","dataPresentIndicator",
        NULL
    };
    for (int i = 0; exact[i]; i++)
        if (strcmp(key, exact[i]) == 0) return 1;
    return 0;
}

typedef enum {
    KEY_SKIP = 0,
    KEY_GROUP_ATTR,
    KEY_DATASET,
    KEY_DATASET_ATTR
} key_role_t;

typedef struct {
    key_role_t role;

    char base_key[512];   /* for group attr or dataset */
    char dset_key[512];   /* left side for key->attr (may be "#n#base") */
    char dset_attr[512];  /* right side for key->attr */

    int native_type;
    size_t size;
} key_class_t;

/*
 * bufr_classify_key
 *     Classify one ecCodes key as a group attribute, dataset, or
 *     dataset attribute according to the BUFR->HDF5 mapping policy.
 *
 * Notes:
 *     Arrow keys (base->attr) may be present but MISSING, so the
 *     function parses them structurally first and treats type/size as
 *     best-effort metadata.
 */
static int bufr_classify_key(codes_handle* h, const char* key, key_class_t* out)
{
    if (!h || !key || !out) return CODES_INVALID_ARGUMENT;
    memset(out, 0, sizeof(*out));
    out->role = KEY_SKIP;

    /* key->attr */
    {
        char left[512], right[512];
        if (split_key_arrow(key, left, sizeof(left), right, sizeof(right))) {
            out->role = KEY_DATASET_ATTR;
            bufr_safe_strcpy(out->dset_key, sizeof(out->dset_key), left);
            bufr_safe_strcpy(out->dset_attr, sizeof(out->dset_attr), right);

            /* best-effort type/size; may fail if MISSING */
            (void)codes_get_native_type(h, key, &out->native_type);
            (void)codes_get_size(h, key, &out->size);
            return 0;
        }
    }

    /* regular key */
    size_t sz = 0;
    int ntype = 0;
    if (codes_get_size(h, key, &sz) != 0) return 0;
    if (codes_get_native_type(h, key, &ntype) != 0) return 0;

    bufr_safe_strcpy(out->base_key, sizeof(out->base_key), key);
    out->native_type = ntype;
    out->size = sz;

    if (is_descriptor_key(key)) {
        out->role = KEY_DATASET;
        return 0;
    }

    if (force_group_attr_key(key) && sz == 1 && ntype != CODES_TYPE_BYTES) {
        out->role = KEY_GROUP_ATTR;
        return 0;
    }

    if (ntype == CODES_TYPE_BYTES) {
        out->role = KEY_DATASET;
        return 0;
    }

    /* header vs data */
    {
        int err = 0;
        int is_header = codes_bufr_key_is_header(h, key, &err);
        if (err != 0) {
            out->role = (sz == 1) ? KEY_GROUP_ATTR : KEY_DATASET;
            return 0;
        }
        out->role = is_header ? ((sz == 1) ? KEY_GROUP_ATTR : KEY_DATASET) : KEY_DATASET;
    }

    return 0;
}

/* ============================================================
 *  Robust post-pass probing for base->meta
 * ============================================================ */

/*
 * probe_any_defined_meta_key
 *     Probe whether a metadata key base->attr exists, including across
 *     replicated forms #n#base->attr.
 *
 * Purpose:
 *     Some metadata keys are not reliably surfaced by the iterator, so
 *     this post-pass probing step is used to synthesize metadata
 *     datasets and attach dataset attributes robustly.
 */
static int probe_any_defined_meta_key(codes_handle* h,
                                      const char* base,
                                      int is_repl,
                                      int rep_count,
                                      const char* attr,
                                      int* out_found,
                                      int* out_native_type,
                                      size_t* out_size,
                                      int* out_found_rep_max)
{
    *out_found = 0;
    if (out_native_type) *out_native_type = 0;
    if (out_size) *out_size = 0;
    if (out_found_rep_max) *out_found_rep_max = 0;

    if (!is_repl) {
        char k[1024];
        snprintf(k, sizeof(k), "%s->%s", base, attr);
        if (codes_is_defined(h, k)) {
            *out_found = 1;
            if (out_native_type) (void)codes_get_native_type(h, k, out_native_type);
            if (out_size) (void)codes_get_size(h, k, out_size);
        }
        return 0;
    }

    /* Replicated: find any k where "#k#base->attr" exists */
    int found_max = 0;
    for (int i = 1; i <= rep_count; i++) {
        char k[1024];
        snprintf(k, sizeof(k), "#%d#%s->%s", i, base, attr);
        if (!codes_is_defined(h, k))
            continue;

        *out_found = 1;
        if (i > found_max) found_max = i;

        /* capture first successful type/size (best-effort; MISSING can still fail) */
        if (out_native_type && *out_native_type == 0) (void)codes_get_native_type(h, k, out_native_type);
        if (out_size && *out_size == 0) (void)codes_get_size(h, k, out_size);
    }

    if (out_found_rep_max) *out_found_rep_max = found_max;
    return 0;
}

/* ============================================================
 *  Inventory build + iteration APIs
 * ============================================================ */

/*
 * bufr_build_group_inventory
 *     Build the inventory for one decoded BUFR message.
 *
 * Pass 1:
 *     Iterate through all keys, classify them, normalize replicated
 *     names, and record explicit dataset attributes.
 *
 * Pass 2:
 *     Probe for whitelisted metadata keys such as units, code, or
 *     percentConfidence and synthesize parallel datasets like
 *     temperature_units.
 */
int bufr_build_group_inventory(codes_handle* h, inv_t* inv)
{
    if (!h || !inv) return CODES_INVALID_ARGUMENT;
    inv_init(inv);

    /* Ensure BUFR decoded keys exist */
    {
        int err = codes_set_long(h, "unpack", 1);
        if (err) return err;
    }

    /* Pass 1: collect group attrs + datasets + replication counts,
     * and record any explicit key->attr we do see. */
    codes_bufr_keys_iterator* it =
        codes_bufr_keys_iterator_new(h, CODES_KEYS_ITERATOR_ALL_KEYS);
    if (!it) return CODES_INTERNAL_ERROR;

    while (codes_bufr_keys_iterator_next(it)) {
        const char* key = codes_bufr_keys_iterator_get_name(it);
        if (!key || !*key) continue;

        key_class_t kc;
        bufr_classify_key(h, key, &kc);

        if (kc.role == KEY_GROUP_ATTR) {

            int rep_n = 0;
            char base[512];
            if (parse_replicated_key(kc.base_key, &rep_n, base, sizeof(base))) {
                inv_dataset_t* d = inv_get_or_add_dataset(inv, base, kc.native_type, kc.size);
                inv_mark_dataset_replicated(d, rep_n);
            } else {
                inv_add_group_attr(inv, kc.base_key, kc.native_type, kc.size);
            }

        } else if (kc.role == KEY_DATASET) {

            int rep_n = 0;
            char base[512];
            if (parse_replicated_key(kc.base_key, &rep_n, base, sizeof(base))) {
                inv_dataset_t* d = inv_get_or_add_dataset(inv, base, kc.native_type, kc.size);
                inv_mark_dataset_replicated(d, rep_n);
            } else {
                (void)inv_get_or_add_dataset(inv, kc.base_key, kc.native_type, kc.size);
            }

        } else if (kc.role == KEY_DATASET_ATTR) {

            /* Keep base dataset name normalized if replicated */
            int rep_n = 0;
            char base[512];
            if (parse_replicated_key(kc.dset_key, &rep_n, base, sizeof(base))) {
                inv_dataset_t* d = inv_get_or_add_dataset(inv, base, 0, 0);
                inv_mark_dataset_replicated(d, rep_n);

                /* record as dataset-attr metadata (type/size may be 0 if MISSING) */
                inv_dset_attr_t* a = inv_get_or_add_dset_attr(d, kc.dset_attr,
                                                              kc.native_type, kc.size);
                inv_mark_attr_per_occurrence(a, rep_n);
            } else {
                inv_dataset_t* d = inv_get_or_add_dataset(inv, kc.dset_key, 0, 0);
                (void)inv_get_or_add_dset_attr(d, kc.dset_attr, kc.native_type, kc.size);
            }
        }
    }

    codes_bufr_keys_iterator_delete(it);

    /* Pass 2 (ROBUST): synthesize meta datasets by PROBING codes_is_defined()
     * for each dataset and each whitelisted meta attr, regardless of iterator output. */
    for (size_t di = 0; di < inv->ndatasets; di++) {
        inv_dataset_t* d = &inv->datasets[di];

        for (int ai = 0; g_known_meta_attrs[ai]; ai++) {
            const char* attr = g_known_meta_attrs[ai];

            int found = 0;
            int ntype = 0;
            size_t sz = 0;
            int found_rep_max = 0;

            (void)probe_any_defined_meta_key(h, d->name,
                                             d->is_replicated, d->rep_count,
                                             attr,
                                             &found, &ntype, &sz, &found_rep_max);

            if (!found)
                continue;

            /* Attach dataset-attr metadata to base dataset */
            inv_dset_attr_t* a = inv_get_or_add_dset_attr(d, attr, ntype, sz);
            if (d->is_replicated) {
                /* mark per-occurrence; rep_count may be larger than found_rep_max */
                inv_mark_attr_per_occurrence(a, (found_rep_max > 0) ? found_rep_max : d->rep_count);
            }

            /* Synthesize parallel dataset base_attr */
            char meta_name[1024];
            make_meta_dataset_name(d->name, attr, meta_name, sizeof(meta_name));

            inv_dataset_t* md = inv_get_or_add_dataset(inv, meta_name, ntype, sz);
            if (d->is_replicated) {
                /* replicate like base (or the max we actually found) */
                inv_mark_dataset_replicated(md, (found_rep_max > 0) ? found_rep_max : d->rep_count);
            }
        }
    }

    return 0;
}

/* Iterate /message_N */
typedef int (*bufr_on_group_attr_fn)(void* uctx, const inv_attr_t* a);
typedef int (*bufr_on_dataset_fn)(void* uctx, const inv_dataset_t* d);

/*
 * bufr_iterate_group
 *     Convenience wrapper that builds the inventory and invokes user
 *     callbacks for every group attribute and dataset.
 */
int bufr_iterate_group(codes_handle* h,
                       bufr_on_group_attr_fn on_attr,
                       bufr_on_dataset_fn on_dset,
                       void* uctx)
{
    if (!h) return CODES_INVALID_ARGUMENT;

    inv_t inv;
    int err = bufr_build_group_inventory(h, &inv);
    if (err) return err;

    int cb = 0;

    if (on_attr) {
        for (size_t i = 0; i < inv.ngroup_attrs; i++) {
            cb = on_attr(uctx, &inv.group_attrs[i]);
            if (cb != 0) { bufr_inv_free(&inv); return cb; }
        }
    }

    if (on_dset) {
        for (size_t i = 0; i < inv.ndatasets; i++) {
            cb = on_dset(uctx, &inv.datasets[i]);
            if (cb != 0) { bufr_inv_free(&inv); return cb; }
        }
    }

    bufr_inv_free(&inv);
    return 0;
}

/* Iterate dataset-attributes (from key->attr) for a normalized dataset name */
typedef int (*bufr_on_dataset_attr_fn)(void* uctx, const inv_dset_attr_t* a);

/*
 * bufr_iterate_dataset_attrs
 *     Build the inventory and invoke a callback for each attribute of
 *     the named dataset. The dataset name must be the normalized HDF5
 *     name, not a replicated ecCodes key.
 */
int bufr_iterate_dataset_attrs(codes_handle* h,
                               const char* dataset_name,
                               bufr_on_dataset_attr_fn on_attr,
                               void* uctx)
{
    if (!h || !dataset_name) return CODES_INVALID_ARGUMENT;

    inv_t inv;
    int err = bufr_build_group_inventory(h, &inv);
    if (err) return err;

    inv_dataset_t* d = inv_find_dataset(&inv, dataset_name);
    if (!d) { bufr_inv_free(&inv); return CODES_NOT_FOUND; }

    int cb = 0;
    if (on_attr) {
        for (size_t i = 0; i < d->nattrs; i++) {
            cb = on_attr(uctx, &d->attrs[i]);
            if (cb != 0) { bufr_inv_free(&inv); return cb; }
        }
    }

    bufr_inv_free(&inv);
    return 0;
}

/* ============================================================
 *  On-demand dataset mapping + replication + reading
 *  (UNCHANGED from your current file)
 * ============================================================ */
#ifdef EP /* it is in bufr_helper.h now */
typedef struct {
    char hdf5_name[512];
    char ecc_key[512];
    int  is_meta_dataset;
    char meta_base[512];
    char meta_attr[256];

    int  is_replicated;
    int  rep_count;

    int    native_type;
    size_t occ_size;
} bufr_dataset_spec_t;
#endif /*EP*/

/*
 * bufr_parse_hdf5_dataset_name
 *     Parse an HDF5 dataset name into the corresponding ecCodes key.
 *
 * Behavior:
 *     Recognizes synthesized metadata datasets of the form base_attr
 *     for whitelisted attributes and converts them back to base->attr.
 */
int bufr_parse_hdf5_dataset_name(const char* hdf5_name, bufr_dataset_spec_t* spec)
{
    if (!hdf5_name || !spec) return CODES_INVALID_ARGUMENT;
    memset(spec, 0, sizeof(*spec));
    bufr_safe_strcpy(spec->hdf5_name, sizeof(spec->hdf5_name), hdf5_name);

    for (int i = 0; g_known_meta_attrs[i]; i++) {
        const char* attr = g_known_meta_attrs[i];
        char suffix[300];
        snprintf(suffix, sizeof(suffix), "_%s", attr);

        if (ends_with(hdf5_name, suffix)) {
            size_t n = strlen(hdf5_name);
            size_t ns = strlen(suffix);
            size_t base_len = n - ns;

            if (base_len == 0 || base_len >= sizeof(spec->meta_base))
                break;

            memcpy(spec->meta_base, hdf5_name, base_len);
            spec->meta_base[base_len] = '\0';

            spec->is_meta_dataset = 1;
            bufr_safe_strcpy(spec->meta_attr, sizeof(spec->meta_attr), attr);
            snprintf(spec->ecc_key, sizeof(spec->ecc_key),
                     "%s->%s", spec->meta_base, spec->meta_attr);
            return 0;
        }
    }

    spec->is_meta_dataset = 0;
    bufr_safe_strcpy(spec->ecc_key, sizeof(spec->ecc_key), hdf5_name);
    return 0;
}

/*
 * bufr_is_replicated_key
 *     Return whether the logical ecCodes key exists in replicated form
 *     by probing for #1#key.
 */
static int bufr_is_replicated_key(codes_handle* h, const char* ecc_key, int* out_is_repl)
{
    if (!h || !ecc_key || !out_is_repl) return CODES_INVALID_ARGUMENT;
    char kname[768];
    snprintf(kname, sizeof(kname), "#1#%s", ecc_key);
    *out_is_repl = codes_is_defined(h, kname) ? 1 : 0;
    return 0;
}

/*
 * bufr_replication_count
 *     Determine the highest contiguous replication index for a key by
 *     probing #1#key, #2#key, ... until the first missing occurrence.
 */
static int bufr_replication_count(codes_handle* h, const char* ecc_key, int* out_rep_count)
{
    if (!h || !ecc_key || !out_rep_count) return CODES_INVALID_ARGUMENT;

    int is_repl = 0;
    int err = bufr_is_replicated_key(h, ecc_key, &is_repl);
    if (err) return err;

    if (!is_repl) {
        *out_rep_count = 0;
        return 0;
    }

    int max_n = 1;
    for (int n = 2; n <= 200000; n++) {
        char kname[768];
        snprintf(kname, sizeof(kname), "#%d#%s", n, ecc_key);
        if (!codes_is_defined(h, kname))
            break;
        max_n = n;
    }

    *out_rep_count = max_n;
    return 0;
}

/*
 * bufr_resolve_dataset_spec
 *     Resolve the parsed dataset specification against the BUFR
 *     message, filling in native type, occurrence size, and replication
 *     information.
 */
int bufr_resolve_dataset_spec(codes_handle* h, bufr_dataset_spec_t* spec)
{
    if (!h || !spec) return CODES_INVALID_ARGUMENT;
 
    { int err = codes_set_long(h, "unpack", 1); if (err) return err; }

    int rep_count = 0;
    int err = bufr_replication_count(h, spec->ecc_key, &rep_count);
    if (err) return err;

    spec->is_replicated = (rep_count > 0) ? 1 : 0;
    spec->rep_count = rep_count;

    char probe[768];
    if (spec->is_replicated) snprintf(probe, sizeof(probe), "#1#%s", spec->ecc_key);
    else bufr_safe_strcpy(probe, sizeof(probe), spec->ecc_key);


    if (!codes_is_defined(h, probe)) return CODES_NOT_FOUND;

    int ntype = 0;
    size_t sz = 0;

    err = codes_get_native_type(h, probe, &ntype);
    if (err) return err;
    err = codes_get_size(h, probe, &sz);
    if (err) return err;

    spec->native_type = ntype;
    spec->occ_size = sz;
    return 0;
}
/* ----- read helpers (long/double/bytes/string) unchanged from your last version ----- */

/*
 * bufr_read_long_dataset
 *     Read a LONG-valued dataset. For replicated keys, data from all
 *     occurrences are concatenated in replication order.
 */
int bufr_read_long_dataset(codes_handle* h, const bufr_dataset_spec_t* spec,
                           long** out_buf, size_t* out_n)
{
    if (!h || !spec || !out_buf || !out_n) return CODES_INVALID_ARGUMENT;
    *out_buf = NULL; *out_n = 0;
    if (spec->native_type != CODES_TYPE_LONG) return CODES_WRONG_TYPE;

    if (!spec->is_replicated) {
        size_t n = 0;
        int err = codes_get_size(h, spec->ecc_key, &n);
        if (err) return err;
        long* buf = (long*)xmalloc(n * sizeof(long));
        err = codes_get_long_array(h, spec->ecc_key, buf, &n);
        if (err) { free(buf); return err; }
        *out_buf = buf; *out_n = n;
        return 0;
    }

    size_t total = 0;
    for (int i = 1; i <= spec->rep_count; i++) {
        char kname[768];
        snprintf(kname, sizeof(kname), "#%d#%s", i, spec->ecc_key);
        if (!codes_is_defined(h, kname)) continue;
        size_t n = 0;
        int err = codes_get_size(h, kname, &n);
        if (err) return err;
        total += n;
    }

    long* buf = (long*)xmalloc(total * sizeof(long));
    size_t off = 0;

    for (int i = 1; i <= spec->rep_count; i++) {
        char kname[768];
        snprintf(kname, sizeof(kname), "#%d#%s", i, spec->ecc_key);
        if (!codes_is_defined(h, kname)) continue;
        size_t n = 0;
        int err = codes_get_size(h, kname, &n);
        if (err) { free(buf); return err; }
        err = codes_get_long_array(h, kname, buf + off, &n);
        if (err) { free(buf); return err; }
        off += n;
    }

    *out_buf = buf; *out_n = total;
    return 0;
}

/*
 * bufr_read_double_dataset
 *     Read a DOUBLE-valued dataset. Replicated occurrences are
 *     concatenated in replication order.
 */
int bufr_read_double_dataset(codes_handle* h, const bufr_dataset_spec_t* spec,
                             double** out_buf, size_t* out_n)
{
    if (!h || !spec || !out_buf || !out_n) return CODES_INVALID_ARGUMENT;
    *out_buf = NULL; *out_n = 0;
    if (spec->native_type != CODES_TYPE_DOUBLE) return CODES_WRONG_TYPE;

    if (!spec->is_replicated) {
        size_t n = 0;
        int err = codes_get_size(h, spec->ecc_key, &n);
        if (err) return err;
        double* buf = (double*)xmalloc(n * sizeof(double));
        err = codes_get_double_array(h, spec->ecc_key, buf, &n);
        if (err) { free(buf); return err; }
        *out_buf = buf; *out_n = n;
        return 0;
    }

    size_t total = 0;
    for (int i = 1; i <= spec->rep_count; i++) {
        char kname[768];
        snprintf(kname, sizeof(kname), "#%d#%s", i, spec->ecc_key);
        if (!codes_is_defined(h, kname)) continue;
        size_t n = 0;
        int err = codes_get_size(h, kname, &n);
        if (err) return err;
        total += n;
    }

    double* buf = (double*)xmalloc(total * sizeof(double));
    size_t off = 0;

    for (int i = 1; i <= spec->rep_count; i++) {
        char kname[768];
        snprintf(kname, sizeof(kname), "#%d#%s", i, spec->ecc_key);
        if (!codes_is_defined(h, kname)) continue;
        size_t n = 0;
        int err = codes_get_size(h, kname, &n);
        if (err) { free(buf); return err; }
        err = codes_get_double_array(h, kname, buf + off, &n);
        if (err) { free(buf); return err; }
        off += n;
    }

    *out_buf = buf; *out_n = total;
    return 0;
}

/*
 * bufr_read_bytes_dataset
 *     Read a BYTES-valued dataset. Replicated byte sequences are
 *     concatenated into a single output buffer.
 */
int bufr_read_bytes_dataset(codes_handle* h, const bufr_dataset_spec_t* spec,
                            unsigned char** out_buf, size_t* out_nbytes)
{
    if (!h || !spec || !out_buf || !out_nbytes) return CODES_INVALID_ARGUMENT;
    *out_buf = NULL; *out_nbytes = 0;
    if (spec->native_type != CODES_TYPE_BYTES) return CODES_WRONG_TYPE;

    if (!spec->is_replicated) {
        size_t n = 0;
        int err = codes_get_size(h, spec->ecc_key, &n);
        if (err) return err;
        unsigned char* buf = (unsigned char*)xmalloc(n);
        err = codes_get_bytes(h, spec->ecc_key, buf, &n);
        if (err) { free(buf); return err; }
        *out_buf = buf; *out_nbytes = n;
        return 0;
    }

    size_t total = 0;
    for (int i = 1; i <= spec->rep_count; i++) {
        char kname[768];
        snprintf(kname, sizeof(kname), "#%d#%s", i, spec->ecc_key);
        if (!codes_is_defined(h, kname)) continue;
        size_t n = 0;
        int err = codes_get_size(h, kname, &n);
        if (err) return err;
        total += n;
    }

    unsigned char* buf = (unsigned char*)xmalloc(total);
    size_t off = 0;

    for (int i = 1; i <= spec->rep_count; i++) {
        char kname[768];
        snprintf(kname, sizeof(kname), "#%d#%s", i, spec->ecc_key);
        if (!codes_is_defined(h, kname)) continue;
        size_t n = 0;
        int err = codes_get_size(h, kname, &n);
        if (err) { free(buf); return err; }
        err = codes_get_bytes(h, kname, buf + off, &n);
        if (err) { free(buf); return err; }
        off += n;
    }

    *out_buf = buf; *out_nbytes = total;
    return 0;
}

/*
 * read_one_string
 *     Read one ecCodes string key into a newly allocated NUL-terminated
 *     buffer sized using codes_get_length().
 */
int read_one_string(codes_handle* h, const char* key, char** out_s)
{
    size_t len = 0;
    int err = codes_get_length(h, key, &len);
    if (err) return err;
    if (len == 0) len = 1;
    char* s = (char*)xmalloc(len);
    size_t cap = len;
    err = codes_get_string(h, key, s, &cap);
    if (err) { free(s); return err; }
    *out_s = s;
    return 0;
}

/*
 * bufr_read_string_dataset
 *     Read a STRING-valued dataset and return an array of individually
 *     allocated strings. For replicated keys, one string is returned
 *     per occurrence that is defined.
 */
int bufr_read_string_dataset(codes_handle* h, const bufr_dataset_spec_t* spec,
                             char*** out_strings, size_t* out_nstrings)
{
    if (!h || !spec || !out_strings || !out_nstrings) return CODES_INVALID_ARGUMENT;
    *out_strings = NULL; *out_nstrings = 0;
    if (spec->native_type != CODES_TYPE_STRING) return CODES_WRONG_TYPE;

    if (!spec->is_replicated) {
        char** arr = (char**)xmalloc(sizeof(char*));
        int err = read_one_string(h, spec->ecc_key, &arr[0]);
        if (err) { free(arr); return err; }
        *out_strings = arr;
        *out_nstrings = 1;
        return 0;
    }

    char** arr = (char**)xmalloc((size_t)spec->rep_count * sizeof(char*));
    size_t nout = 0;

    for (int i = 1; i <= spec->rep_count; i++) {
        char kname[768];
        snprintf(kname, sizeof(kname), "#%d#%s", i, spec->ecc_key);
        if (!codes_is_defined(h, kname)) continue;

        char* s = NULL;
        int err = read_one_string(h, kname, &s);
        if (err) {
            for (size_t j = 0; j < nout; j++) free(arr[j]);
            free(arr);
            return err;
        }
        arr[nout++] = s;
    }

    *out_strings = arr;
    *out_nstrings = nout;
    return 0;
}

/* ============================================================
 *  Optional demo
 * ============================================================ */

/*
 * die_err
 *     Small demo helper that prints an ecCodes error message and exits.
 */
static void die_err(const char* where, int err)
{
    fprintf(stderr, "%s: %s\n", where, codes_get_error_message(err));
    exit(1);
}

/*
 * open_bufr_message_by_index
 *     Demo helper that opens the Nth BUFR message from a file stream.
 */
static codes_handle* open_bufr_message_by_index(FILE* f, long target)
{
    int err = 0;
    long idx = 0;

    while (1) {
        codes_handle* h = codes_handle_new_from_file(NULL, f, PRODUCT_BUFR, &err);
        if (!h) {
            if (err == CODES_END_OF_FILE) return NULL;
            die_err("codes_handle_new_from_file", err);
        }
        if (idx == target) return h;
        codes_handle_delete(h);
        idx++;
    }
}
/*
 * on_gattr / on_dset
 *     Demo callbacks used by the example main() when iterating over the
 *     inventory.
 */
static int on_gattr(void* uctx, const inv_attr_t* a)
{
    (void)uctx;
    printf("GATTR %s (type=%d size=%zu)\n", a->name, a->native_type, a->size);
    return 0;
}

static int on_dset(void* uctx, const inv_dataset_t* d)
{
    (void)uctx;
    if (d->is_replicated) {
        printf("DSET  %s (type=%d occ_size=%zu) REPL rep_count=%d attrs=%zu\n",
               d->name, d->native_type, d->occ_size, d->rep_count, d->nattrs);
    } else {
        printf("DSET  %s (type=%d occ_size=%zu) attrs=%zu\n",
               d->name, d->native_type, d->occ_size, d->nattrs);
    }
    for (size_t i = 0; i < d->nattrs; i++) {
        const inv_dset_attr_t* a = &d->attrs[i];
        if (a->per_occurrence) {
            printf("      ->%s (type=%d size=%zu) PER_OCC rep_count=%d\n",
                   a->name, a->native_type, a->size, a->rep_count);
        } else {
            printf("      ->%s (type=%d size=%zu)\n",
                   a->name, a->native_type, a->size);
        }
    }
    return 0;
}

/*
 * demo_read_dataset
 *     Example routine that resolves a dataset name and prints a preview
 *     of the values returned by the type-specific read helpers.
 */
static void demo_read_dataset(codes_handle* h, const char* name)
{
    bufr_dataset_spec_t spec;
    int err = bufr_parse_hdf5_dataset_name(name, &spec);
    if (err) die_err("bufr_parse_hdf5_dataset_name", err);

    err = bufr_resolve_dataset_spec(h, &spec);
    if (err) die_err("bufr_resolve_dataset_spec", err);

    printf("\nREAD %s\n", name);
    printf("  ecc_key=%s type=%d occ_size=%zu repl=%d rep_count=%d\n",
           spec.ecc_key, spec.native_type, spec.occ_size, spec.is_replicated, spec.rep_count);

    if (spec.native_type == CODES_TYPE_DOUBLE) {
        double* buf = NULL; size_t n = 0;
        err = bufr_read_double_dataset(h, &spec, &buf, &n);
        if (err) die_err("bufr_read_double_dataset", err);
        printf("  got %zu doubles (first 16):\n", n);
        for (size_t i = 0; i < n && i < 16; i++) printf("    [%zu]=%.17g\n", i, buf[i]);
        free(buf);

    } else if (spec.native_type == CODES_TYPE_LONG) {
        long* buf = NULL; size_t n = 0;
        err = bufr_read_long_dataset(h, &spec, &buf, &n);
        if (err) die_err("bufr_read_long_dataset", err);
        printf("  got %zu longs (first 16):\n", n);
        for (size_t i = 0; i < n && i < 16; i++) printf("    [%zu]=%ld\n", i, buf[i]);
        free(buf);

    } else if (spec.native_type == CODES_TYPE_STRING) {
        char** arr = NULL; size_t n = 0;
        err = bufr_read_string_dataset(h, &spec, &arr, &n);
        if (err) die_err("bufr_read_string_dataset", err);
        printf("  got %zu strings:\n", n);
        for (size_t i = 0; i < n; i++) { printf("    [%zu]=%s\n", i, arr[i]); free(arr[i]); }
        free(arr);

    } else if (spec.native_type == CODES_TYPE_BYTES) {
        unsigned char* buf = NULL; size_t n = 0;
        err = bufr_read_bytes_dataset(h, &spec, &buf, &n);
        if (err) die_err("bufr_read_bytes_dataset", err);
        printf("  got %zu bytes (first 32):\n    ", n);
        for (size_t i = 0; i < n && i < 32; i++) printf("%02x ", (unsigned)buf[i]);
        printf("\n");
        free(buf);

    } else {
        printf("  unsupported native type %d\n", spec.native_type);
    }
}



/*
 * demo_print_scalar_or_array_value
 *     Demo helper that prints a scalar or short preview of an ecCodes
 *     key value using the supplied native type.
 */
static void demo_print_scalar_or_array_value(codes_handle* h,
                                             const char* key,
                                             int native_type,
                                             size_t size)
{
    int err;

    if (native_type == CODES_TYPE_LONG) {
        size_t n = (size > 0) ? size : 1;
        long* buf = (long*)xmalloc(n * sizeof(long));
        err = codes_get_long_array(h, key, buf, &n);
        if (err) {
            free(buf);
            die_err("codes_get_long_array", err);
        }
        if (n == 1)
            printf("  value=%ld\n", buf[0]);
        else {
            printf("  %zu longs (first 16):\n", n);
            for (size_t i = 0; i < n && i < 16; i++)
                printf("    [%zu]=%ld\n", i, buf[i]);
        }
        free(buf);

    } else if (native_type == CODES_TYPE_DOUBLE) {
        size_t n = (size > 0) ? size : 1;
        double* buf = (double*)xmalloc(n * sizeof(double));
        err = codes_get_double_array(h, key, buf, &n);
        if (err) {
            free(buf);
            die_err("codes_get_double_array", err);
        }
        if (n == 1)
            printf("  value=%.17g\n", buf[0]);
        else {
            printf("  %zu doubles (first 16):\n", n);
            for (size_t i = 0; i < n && i < 16; i++)
                printf("    [%zu]=%.17g\n", i, buf[i]);
        }
        free(buf);

    } else if (native_type == CODES_TYPE_STRING) {
        char* s = NULL;
        err = read_one_string(h, key, &s);
        if (err) die_err("read_one_string", err);
        printf("  value=%s\n", s);
        free(s);

    } else if (native_type == CODES_TYPE_BYTES) {
        size_t n = (size > 0) ? size : 1;
        unsigned char* buf = (unsigned char*)xmalloc(n);
        err = codes_get_bytes(h, key, buf, &n);
        if (err) {
            free(buf);
            die_err("codes_get_bytes", err);
        }
        printf("  %zu bytes (first 32):\n    ", n);
        for (size_t i = 0; i < n && i < 32; i++)
            printf("%02x ", (unsigned)buf[i]);
        printf("\n");
        free(buf);

    } else {
        printf("  unsupported native type %d\n", native_type);
    }
}

/*
 * demo_read_dataset_attr
 *     Example routine that reads one attribute associated with a
 *     normalized dataset name. For replicated datasets it probes
 *     #n#base->attr and prints each defined occurrence separately.
 */
static void demo_read_dataset_attr(codes_handle* h,
                                   const char* dataset_name,
                                   const char* attr_name)
{
    inv_t inv;
    int err = bufr_build_group_inventory(h, &inv);
    if (err) die_err("bufr_build_group_inventory", err);

    inv_dataset_t* d = inv_find_dataset(&inv, dataset_name);
    if (!d) {
        bufr_inv_free(&inv);
        fprintf(stderr, "Dataset not found: %s\n", dataset_name);
        exit(1);
    }

    inv_dset_attr_t* a = inv_find_dset_attr(d, attr_name);
    if (!a) {
        bufr_inv_free(&inv);
        fprintf(stderr, "Dataset attribute not found: %s->%s\n", dataset_name, attr_name);
        exit(1);
    }

    printf("\nREAD ATTR %s->%s\n", dataset_name, attr_name);

    if (d->is_replicated) {
        int found_any = 0;
        for (int i = 1; i <= d->rep_count; i++) {
            char key[1024];
            snprintf(key, sizeof(key), "#%d#%s->%s", i, dataset_name, attr_name);
            if (!codes_is_defined(h, key))
                continue;

            size_t sz = a->size;
            if (sz == 0)
                (void)codes_get_size(h, key, &sz);

            printf("  occurrence %d key=%s\n", i, key);
            demo_print_scalar_or_array_value(h, key, a->native_type, sz);
            found_any = 1;
        }
        if (!found_any) {
            bufr_inv_free(&inv);
            fprintf(stderr, "No defined replicated occurrences for %s->%s\n", dataset_name, attr_name);
            exit(1);
        }
    } else {
        char key[1024];
        snprintf(key, sizeof(key), "%s->%s", dataset_name, attr_name);
        if (!codes_is_defined(h, key)) {
            bufr_inv_free(&inv);
            fprintf(stderr, "Attribute key not defined: %s\n", key);
            exit(1);
        }

        size_t sz = a->size;
        if (sz == 0)
            (void)codes_get_size(h, key, &sz);

        printf("  key=%s\n", key);
        demo_print_scalar_or_array_value(h, key, a->native_type, sz);
    }

    bufr_inv_free(&inv);
}

/*
 * demo_read_group_attr
 *     Example routine that reads one group attribute directly from the
 *     BUFR message using its native ecCodes type.
 */
static void demo_read_group_attr(codes_handle* h, const char* attr_name)
{
    inv_t inv;
    int err = bufr_build_group_inventory(h, &inv);
    if (err) die_err("bufr_build_group_inventory", err);

    const inv_attr_t* a = NULL;
    for (size_t i = 0; i < inv.ngroup_attrs; i++) {
        if (strcmp(inv.group_attrs[i].name, attr_name) == 0) {
            a = &inv.group_attrs[i];
            break;
        }
    }

    if (!a) {
        bufr_inv_free(&inv);
        fprintf(stderr, "Group attribute not found: %s\n", attr_name);
        exit(1);
    }

    if (!codes_is_defined(h, attr_name)) {
        bufr_inv_free(&inv);
        fprintf(stderr, "Group attribute key not defined: %s\n", attr_name);
        exit(1);
    }

    printf("\nREAD GATTR %s\n", attr_name);
    demo_print_scalar_or_array_value(h, attr_name, a->native_type, a->size);

    bufr_inv_free(&inv);
}
#ifdef DEMO
/*
 * main
 *     Optional standalone demo:
 *       1. open the requested BUFR message
 *       2. build and print the inventory
 *       3. optionally read one named dataset
 */
int main(int argc, char** argv)
{
    if (argc != 3 && argc != 4 && argc != 5) {
        fprintf(stderr, "Usage:  %s <file.bufr> <msg_index> <dataset_name> <attr_name> \n", argv[0]);
        fprintf(stderr, "Usage:  %s <file.bufr> <msg_index> --gattr <attr_name> \n", argv[0]);
        return 2;
    }

    inv_t inv;
    const char* path = argv[1];
    long msg_index = strtol(argv[2], NULL, 10);

    FILE* f = fopen(path, "rb");
    if (!f) { perror("fopen"); return 1; }

    codes_handle* h = open_bufr_message_by_index(f, msg_index);
    if (!h) { fprintf(stderr, "Message %ld not found ", msg_index); fclose(f); return 1; }

    int err = bufr_build_group_inventory(h, &inv);
    if (err) {
        fprintf(stderr, "Inventory build failed ");
        codes_handle_delete(h);
        fclose(f);
        return err;
    }

    bufr_inv_print(&inv);
    bufr_inv_free(&inv);
/*
    printf("=== INVENTORY /message_%ld === ", msg_index);
    err = bufr_iterate_group(h, on_gattr, on_dset, NULL);
    if (err) die_err("bufr_iterate_group", err);
*/
    if (argc == 4) {
        demo_read_dataset(h, argv[3]);
    } else if (argc == 5) {
        if (strcmp(argv[3], "--gattr") == 0)
            demo_read_group_attr(h, argv[4]);
        else
            demo_read_dataset_attr(h, argv[3], argv[4]);
    }

    codes_handle_delete(h);
    fclose(f);
    return 0;
}
#endif /* DEMO */

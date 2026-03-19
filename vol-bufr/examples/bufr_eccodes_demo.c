#include "bufr_helper.h"

#include <eccodes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void
die_err(const char *where, int err)
{
    fprintf(stderr, "%s: %s\n", where, codes_get_error_message(err));
    exit(1);
}

static codes_handle *
open_bufr_message_by_index(FILE *f, long target)
{
    int  err = 0;
    long idx = 0;

    while (1) {
        codes_handle *h = codes_handle_new_from_file(NULL, f, PRODUCT_BUFR, &err);
        if (!h) {
            if (err == CODES_END_OF_FILE)
                return NULL;
            die_err("codes_handle_new_from_file", err);
        }

        if (idx == target)
            return h;

        codes_handle_delete(h);
        idx++;
    }
}

static const inv_dataset_t *
find_dataset(const inv_t *inv, const char *name)
{
    if (!inv || !name)
        return NULL;

    for (size_t i = 0; i < inv->ndatasets; i++) {
        if (strcmp(inv->datasets[i].name, name) == 0)
            return &inv->datasets[i];
    }

    return NULL;
}

static const inv_dset_attr_t *
find_dataset_attr(const inv_dataset_t *d, const char *name)
{
    if (!d || !name)
        return NULL;

    for (size_t i = 0; i < d->nattrs; i++) {
        if (strcmp(d->attrs[i].name, name) == 0)
            return &d->attrs[i];
    }

    return NULL;
}

static const inv_attr_t *
find_group_attr(const inv_t *inv, const char *name)
{
    if (!inv || !name)
        return NULL;

    for (size_t i = 0; i < inv->ngroup_attrs; i++) {
        if (strcmp(inv->group_attrs[i].name, name) == 0)
            return &inv->group_attrs[i];
    }

    return NULL;
}

static void
print_value_preview(codes_handle *h, const char *key, int native_type, size_t size_hint)
{
    int err;

    if (native_type == CODES_TYPE_LONG) {
        size_t n = size_hint ? size_hint : 1;
        long  *buf = (long *)xmalloc(n * sizeof(long));

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
    }
    else if (native_type == CODES_TYPE_DOUBLE) {
        size_t  n = size_hint ? size_hint : 1;
        double *buf = (double *)xmalloc(n * sizeof(double));

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
    }
    else if (native_type == CODES_TYPE_STRING) {
        char *s = NULL;

        err = read_one_string(h, key, &s);
        if (err)
            die_err("read_one_string", err);

        printf("  value=%s\n", s ? s : "(null)");
        free(s);
    }
    else if (native_type == CODES_TYPE_BYTES) {
        size_t         n = size_hint ? size_hint : 1;
        unsigned char *buf = (unsigned char *)xmalloc(n);

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
    }
    else {
        printf("  unsupported native type %d\n", native_type);
    }
}

static void
demo_read_dataset(codes_handle *h, const char *name)
{
    bufr_dataset_spec_t spec;
    int                 err;

    err = bufr_parse_hdf5_dataset_name(name, &spec);
    if (err)
        die_err("bufr_parse_hdf5_dataset_name", err);

    err = bufr_resolve_dataset_spec(h, &spec);
    if (err)
        die_err("bufr_resolve_dataset_spec", err);

    printf("\nREAD DSET %s\n", name);
    printf("  ecc_key=%s type=%d occ_size=%zu replicated=%d rep_count=%d\n",
           spec.ecc_key, spec.native_type, spec.occ_size,
           spec.is_replicated, spec.rep_count);

    if (spec.native_type == CODES_TYPE_LONG) {
        long   *buf = NULL;
        size_t  n = 0;

        err = bufr_read_long_dataset(h, &spec, &buf, &n);
        if (err)
            die_err("bufr_read_long_dataset", err);

        printf("  got %zu longs (first 16):\n", n);
        for (size_t i = 0; i < n && i < 16; i++)
            printf("    [%zu]=%ld\n", i, buf[i]);
        free(buf);
    }
    else if (spec.native_type == CODES_TYPE_DOUBLE) {
        double *buf = NULL;
        size_t  n = 0;

        err = bufr_read_double_dataset(h, &spec, &buf, &n);
        if (err)
            die_err("bufr_read_double_dataset", err);

        printf("  got %zu doubles (first 16):\n", n);
        for (size_t i = 0; i < n && i < 16; i++)
            printf("    [%zu]=%.17g\n", i, buf[i]);
        free(buf);
    }
    else if (spec.native_type == CODES_TYPE_STRING) {
        char  **arr = NULL;
        size_t  n = 0;

        err = bufr_read_string_dataset(h, &spec, &arr, &n);
        if (err)
            die_err("bufr_read_string_dataset", err);

        printf("  got %zu strings:\n", n);
        for (size_t i = 0; i < n; i++) {
            printf("    [%zu]=%s\n", i, arr[i] ? arr[i] : "(null)");
            free(arr[i]);
        }
        free(arr);
    }
    else if (spec.native_type == CODES_TYPE_BYTES) {
        unsigned char *buf = NULL;
        size_t         n = 0;

        err = bufr_read_bytes_dataset(h, &spec, &buf, &n);
        if (err)
            die_err("bufr_read_bytes_dataset", err);

        printf("  got %zu bytes (first 32):\n    ", n);
        for (size_t i = 0; i < n && i < 32; i++)
            printf("%02x ", (unsigned)buf[i]);
        printf("\n");
        free(buf);
    }
    else {
        printf("  unsupported native type %d\n", spec.native_type);
    }
}

static void
demo_read_group_attr(codes_handle *h, const inv_t *inv, const char *attr_name)
{
    const inv_attr_t *a = find_group_attr(inv, attr_name);

    if (!a) {
        fprintf(stderr, "Group attribute not found: %s\n", attr_name);
        exit(1);
    }

    if (!codes_is_defined(h, attr_name)) {
        fprintf(stderr, "Group attribute key not defined: %s\n", attr_name);
        exit(1);
    }

    printf("\nREAD GATTR %s\n", attr_name);
    print_value_preview(h, attr_name, a->native_type, a->size);
}

static void
demo_read_dataset_attr(codes_handle *h, const inv_t *inv, const char *dataset_name,
                       const char *attr_name)
{
    const inv_dataset_t   *d = find_dataset(inv, dataset_name);
    const inv_dset_attr_t *a;
    size_t                 sz;
    char                   key[1024];
    int                    found_any = 0;

    if (!d) {
        fprintf(stderr, "Dataset not found in inventory: %s\n", dataset_name);
        exit(1);
    }

    a = find_dataset_attr(d, attr_name);
    if (!a) {
        fprintf(stderr, "Dataset attribute not found: %s->%s\n", dataset_name, attr_name);
        exit(1);
    }

    printf("\nREAD DATTR %s->%s\n", dataset_name, attr_name);

    if (d->is_replicated && a->per_occurrence) {
        for (int i = 1; i <= d->rep_count; i++) {
            snprintf(key, sizeof(key), "#%d#%s->%s", i, dataset_name, attr_name);
            if (!codes_is_defined(h, key))
                continue;

            sz = a->size;
            if (sz == 0)
                (void)codes_get_size(h, key, &sz);

            printf("  occurrence %d key=%s\n", i, key);
            print_value_preview(h, key, a->native_type, sz);
            found_any = 1;
        }

        if (!found_any) {
            fprintf(stderr, "No defined replicated occurrences for %s->%s\n",
                    dataset_name, attr_name);
            exit(1);
        }
    }
    else {
        snprintf(key, sizeof(key), "%s->%s", dataset_name, attr_name);
        if (!codes_is_defined(h, key)) {
            fprintf(stderr, "Attribute key not defined: %s\n", key);
            exit(1);
        }

        sz = a->size;
        if (sz == 0)
            (void)codes_get_size(h, key, &sz);

        printf("  key=%s\n", key);
        print_value_preview(h, key, a->native_type, sz);
    }
}

static void
print_usage(const char *prog)
{
    fprintf(stderr, "Usage:\n");
    fprintf(stderr, "  %s <file.bufr> <msg_index>\n", prog);
    fprintf(stderr, "  %s <file.bufr> <msg_index> <dataset_name>\n", prog);
    fprintf(stderr, "  %s <file.bufr> <msg_index> --gattr <attr_name>\n", prog);
    fprintf(stderr, "  %s <file.bufr> <msg_index> <dataset_name> <attr_name>\n", prog);
}

int
main(int argc, char **argv)
{
    const char   *path;
    long          msg_index;
    FILE         *f = NULL;
    codes_handle *h = NULL;
    inv_t         inv;
    int           err;

    if (argc != 3 && argc != 4 && argc != 5) {
        print_usage(argv[0]);
        return 2;
    }

    path = argv[1];
    msg_index = strtol(argv[2], NULL, 10);

    f = fopen(path, "rb");
    if (!f) {
        perror("fopen");
        return 1;
    }

    h = open_bufr_message_by_index(f, msg_index);
    if (!h) {
        fprintf(stderr, "Message %ld not found\n", msg_index);
        fclose(f);
        return 1;
    }

    err = bufr_build_group_inventory(h, &inv);
    if (err) {
        fprintf(stderr, "Inventory build failed: %s\n", codes_get_error_message(err));
        codes_handle_delete(h);
        fclose(f);
        return 1;
    }

    bufr_inv_print(&inv);

    if (argc == 4) {
        demo_read_dataset(h, argv[3]);
    }
    else if (argc == 5) {
        if (strcmp(argv[3], "--gattr") == 0)
            demo_read_group_attr(h, &inv, argv[4]);
        else
            demo_read_dataset_attr(h, &inv, argv[3], argv[4]);
    }

    bufr_inv_free(&inv);
    codes_handle_delete(h);
    fclose(f);
    return 0;
}

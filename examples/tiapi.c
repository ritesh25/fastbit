/*
 File: $Id$
 Author: John Wu <John.Wu at acm.org>
      Lawrence Berkeley National Laboratory
 Copyright (c) 2014-2015 the Regents of the University of California
*/
/**
   @file tiapi.c

   A simple test program for functions defined in iapi.h.

    @ingroup FastBitExamples
*/
#include <iapi.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <string.h>

static int msglvl=0;
void usage(const char *name) {
    fprintf(stdout, "A simple tester for the in-memory API of %s\n\nusage\n"
	    "%s [maxsize] [verboseness-level]",
            fastbit_get_version_string(), name);
} /* usage */

/** A simple reader to be used by FastBit for index reconstruction.  In
    this simple case, the first argument is the whole array storing all the
    serialized bitmaps.  This first argument can be used to point to a data
    structure pointing to any complex object type necassary.
 */
static int mybmreader(void *ctx, uint64_t start, uint64_t count, uint32_t *buf) {
    unsigned j;
    const uint32_t *bms = (uint32_t*)ctx + start;
    for (j = 0; j < count; ++ j)
        buf[j] = bms[j];
    return 0;
} // mybmreader

typedef struct {
    uint64_t n;
    double *ptr;
} dblbuffer;
static int mydblreader(void *ctx, uint64_t nd, uint64_t *starts,
                       uint64_t *counts, void *data) {
    size_t j;
    double *out = (double*)data;
    dblbuffer *db = (dblbuffer*)ctx;

    if (ctx == 0 || nd != 1 || starts == 0 || counts == 0 || data == 0)
        return -1;

    if (*starts >= db->n) {
        *counts = 0;
        return 0; // nothing to copy
    }

    if (db->n - *starts < *counts)
        *counts = db->n - *starts;
    for (j = 0; j < *counts; ++ j)
        out[j] = db->ptr[j + *starts];
    return 0;
} // mydblread

/** Generate three arrays of specified sizes.
    The three arrays have the following values:
    - a1 has non-negative 16-bit integers going from 0 to 32637 and then
      repeat.
    - a2 contains 32-bit signed integer.  It increaments every two steps.
    - a3 contains 64-bit floating-point numbers that goes from 0 in
      increment of 1/4.
 */
static void fillarrays(size_t n, int16_t *a1, int32_t *a2, double *a3) {
    size_t j;
    for (j = 0; j < n; ++ j) {
        a1[j] = (j & 0x7FFFU);
        a2[j] = (j >> 1);
        a3[j] = j * 0.25;
    }
} /* fillarrays */

static void queryarrays(size_t n, int16_t *a1, int32_t *a2, double *a3) {
    int16_t b1 = 5;
    int32_t b2 = 11;
    double b31 = 2.0, b32 = 3.5;
    long int i, ierr;
    /* NOTE: keys, offsets and bms are used by FastBit after invoking
       fastbit_iapi_attach_index; they can only be freed after the index is
       no longer needed */
    double *keys;
    int64_t *offsets;
    uint32_t *bms;
    FastBitSelectionHandle h1, h2, h3, h4, h5;
    dblbuffer dbl;
    dbl.n = n;
    dbl.ptr = a3;
    printf("\n*** queryarrays - starting with n=%lu ***\n", n);

    ierr = fastbit_iapi_register_array("a1", FastBitDataTypeShort, a1, n);
    if (ierr < 0) {
        printf("Warning -- fastbit_iapi_register_array failed to register a1, "
               "ierr = %ld\n", ierr);
        fflush(stdout);
        return;
    }
    ierr = fastbit_iapi_register_array("a2", FastBitDataTypeInt, a2, n);
    if (ierr < 0) {
        printf("Warning -- fastbit_iapi_register_array failed to register a2, "
               "ierr = %ld\n", ierr);
        fflush(stdout);
        return;
    }
    ierr = fastbit_iapi_register_array_ext
        ("a3", FastBitDataTypeDouble, &(dbl.n), 1, &dbl, mydblreader);
    if (ierr < 0) {
        printf("Warning -- fastbit_iapi_register_array failed to register a3, "
               "ierr = %ld\n", ierr);
        fflush(stdout);
        return;
    }

    /* build index on a1, automatically attached */
    ierr = fastbit_iapi_build_index("a1", "<binning none/>");
    if (ierr < 0) {
        printf("Warning -- fastbit_iapi_build_index failed to create index "
               "for a1, ierr=%ld\n", ierr);
        fflush(stdout);
    }

    h1 = fastbit_selection_osr("a1", FastBitCompareLess, b1);
    ierr = fastbit_selection_estimate(h1);
    if (ierr < 0) {
        printf("Warning -- fastbit_selection_estimate(a1 < %d) returned %ld\n",
                (int)b1, ierr);
        fflush(stdout);
    }
    ierr = fastbit_selection_evaluate(h1);
    fflush(stdout);
    if (ierr < 0) {
        printf("Warning -- fastbit_selection_evaluate(a1 < %d) returned %ld\n",
                (int)b1, ierr);
        fflush(stdout);
    }
    else {
        long int n1 = ierr;
        int16_t *buf1 = (int16_t*)malloc(2*n1);
        double  *buf3 = (double*)malloc(8*n1);
        long int expected = (n & 0x7FFFL);
        if (expected > b1)
            expected = b1;
        expected += b1 * (n >> 15);
        if (ierr != expected) {
            printf("Warning -- fastbit_selection_evaluate(a1 < %d) expected "
                   "%ld, but got %ld\n", (int)b1, expected, ierr);
            fflush(stdout);
        }
        else {
            printf("fastbit_selection_evaluate(a1 < %d) returned %ld as "
                   "expected\n", (int)b1, ierr);
            fflush(stdout);
        }

        if (n1 > 0) {
            ierr = fastbit_selection_read
                (FastBitDataTypeShort, a1, n, h1, buf1, n1, 0U);
            if (ierr != n1) {
                printf("Warning -- fastbit_selection_read expected to read %ld "
                       "element(s) of a1, but %ld\n", n1, ierr);
                fflush(stdout);
            }
            if (ierr > 0) {
                printf("read a1 where (a1 < %d) got:", (int)b1);
                for (i = 0; i < ierr; ++ i)
                    printf(" %d", (int)buf1[i]);
                printf("\n");
            }

            ierr = fastbit_selection_read
                (FastBitDataTypeDouble, a3, n, h1, buf3, n1, 0U);
            if (ierr != n1) {
                printf("Warning -- fastbit_selection_read expected to read %ld "
                       "element(s) of a3, but %ld\n", n1, ierr);
                fflush(stdout);
            }
            if (ierr > 0) {
                printf("read a3 where (a1 < %d) got:", (int)b1);
                for (i = 0; i < ierr; ++ i)
                    printf(" %.2lf", buf3[i]);
                printf("\n\n");
                fflush(stdout);
            }
        }
        free(buf1);
        free(buf3);
    }
    fastbit_selection_free(h1);


    { /* Serialize the index for a1 and re-attach it */
        uint64_t nk, nf, nb, nv=n;
        ierr = fastbit_iapi_deconstruct_index
            ("a1", &keys, &nk, &offsets, &nf, &bms, &nb);
        if (ierr >= 0) {
            if (msglvl > 3) {
                fflush(stdout);
                printf("fastbit_iapi_deconstruct_index returned nk=%lld, "
                       "nf=%lld, and nb=%lld\n", nk, nf, nb);
                if (nk==2*(nf-1)) { // binned index
                    for (i = 0; i < nf-1; ++ i) {
                        printf("[%lg, %lg) [%lli, %lli)\n", keys[i],
                               keys[nf-1+i], offsets[i], offsets[i+1]);
                    }
                }
                else { // unbinned
                    for (i = 0; i < nf-1; ++ i) {
                        printf("[%lg] [%lli, %lli)\n", keys[i],
                               offsets[i], offsets[i+1]);
                    }
                }
                fflush(stdout);
            }

            fastbit_iapi_free_array("a1");
            ierr = fastbit_iapi_register_array_index_only
                ("a1", FastBitDataTypeShort, &nv, 1,
                 keys, nk, offsets, nf, bms, mybmreader);
            if (ierr < 0) {
                fflush(stdout);
                printf("Warning -- fastbit_iapi_attach_index failed "
                       "to attach the index to a1, ierr = %ld\n", ierr);
                fflush(stdout);
            }
        }
        else {
            fflush(stdout);
            printf("Warning -- fastbit_iapi_deconstruct_index failed "
                   "to serialize the index of a1, ierr = %ld\n", ierr);
            fflush(stdout);
        }
    }

    ierr = fastbit_iapi_build_index("a2", (const char*)0);
    if (ierr < 0) {
        fflush(stdout);
        printf("Warning -- fastbit_iapi_build_index failed to create index "
               "for a2, ierr=%ld\n", ierr);
        fflush(stdout);
    }
    /* a1 < b1 */
    h1 = fastbit_selection_osr("a1", FastBitCompareLess, b1);
    /* a2 <= b2 */
    h2 = fastbit_selection_osr("a2", FastBitCompareLessEqual, b2);
    /* b31 <= a3 < b32 */
    h3 = fastbit_selection_combine
        (fastbit_selection_osr("a3", FastBitCompareGreaterEqual, b31),
         FastBitCombineAnd,
         fastbit_selection_osr("a3", FastBitCompareLess, b32));
    /* a1 < b1 OR b31 <= a3 < b32 */
    h4 = fastbit_selection_combine(h1, FastBitCombineOr, h3);
    /* a2 <= b2 AND (a1 < b1 OR b31 <= a3 < b32) */
    h5 = fastbit_selection_combine(h2, FastBitCombineAnd, h4);

    ierr = fastbit_selection_evaluate(h5);
    if (ierr < 0) {
        fflush(stdout);
        printf("Warning -- fastbit_selection_evaluate(...) returned %ld\n",
               ierr);
        fflush(stdout);
    }
    else {
        long int n1 = ierr;
        int16_t *buf1 = (int16_t*)malloc(2*n1);
        double  *buf3 = (double*)malloc(8*n1);
        long int expected = (n < b1 ? n : b1);

        if (n > 7) {
            if (n > 13) {
                expected += 6;
            }
            else {
                expected += (n - 7);
            }
        }
        if (ierr != expected) {
            printf("Warning -- fastbit_selection_evaluate(...) expected %ld, "
                   "but got %ld\n", expected, ierr);
            fflush(stdout);
        }
        else {
            printf("fastbit_selection_evaluate(...) returned %ld as expected\n",
                   ierr);
            fflush(stdout);
        }

        if (n1 > 0) {
            ierr = fastbit_selection_read
                (FastBitDataTypeShort, a1, n, h5, buf1, n1, 0U);
            if (ierr != n1) {
                printf("Warning -- fastbit_selection_read expected to read %ld "
                       "element(s) of a1, but %ld\n", n1, ierr);
                fflush(stdout);
            }
            if (ierr > 0) {
                printf("read a1 where (...) got:");
                for (i = 0; i < ierr; ++ i)
                    printf(" %d", (int)buf1[i]);
                printf("\n");
                fflush(stdout);
            }

            ierr = fastbit_selection_read
                (FastBitDataTypeDouble, a3, n, h5, buf3, n1, 0U);
            if (ierr != n1) {
                printf("Warning -- fastbit_selection_read expected to read %ld "
                       "element(s) of a3, but %ld\n", n1, ierr);
                fflush(stdout);
            }
            if (ierr > 0) {
                printf("read a3 where (a1 < %d) got:", (int)b1);
                for (i = 0; i < ierr; ++ i)
                    printf(" %.2lf", buf3[i]);
                printf("\n\n");
                fflush(stdout);
            }
        }

        free(buf1);
        free(buf3);
    }
    fastbit_selection_free(h5);
    fastbit_iapi_free_all();
    free(offsets);
    free(keys);
    free(bms);
} /* queryarrays */

/** Generate three arrays of specified sizes.
    The three arrays have the following values:
    - a1 has all 0s.  will be treated as literal bits, FastBitDataTypeBitRaw
    - a2 has all 1s.  will be treated as literal bits, FastBitDataTypeBitRaw
    - a3 has 2s in the first half, and 3s in the second half.
 */
static void fillarray2(size_t n, void *a1, void *a2, double *a3) {
    size_t j;
    for (j = 0; j < n/2; ++ j) {
        a3[j] = 2.0;
    }
    for (j = n/2; j < n; ++ j) {
        a3[j] = 3.0;
    }
    (void) memset(a1, 0, (n+7)/8);
    (void) memset(a2, 0xFF, (n+7)/8);
} /* fillarray2 */

static void queryarray2(size_t n, void *a1, void *a2, double *a3) {
    int16_t b1 = 5;
    int32_t b2 = 11;
    double  b31 = 2.0, b32 = 3;
    long int i, ierr;
    /* NOTE: keys, offsets and bms are used by FastBit after invoking
       fastbit_iapi_attach_index; they can only be freed after the index is
       no longer needed */
    double *keys3;
    int64_t *offsets3;
    uint32_t *bms3;
    FastBitSelectionHandle h1, h2, h3, h4, h5;
    dblbuffer dbl;
    dbl.n = n;
    dbl.ptr = a3;
    printf("\n*** queryarray2 - starting with n=%lu ***\n", n);

    ierr = fastbit_iapi_register_array("a1", FastBitDataTypeBitRaw, a1, n);
    if (ierr < 0) {
        printf("Warning -- fastbit_iapi_register_array failed to register a1, "
               "ierr = %ld\n", ierr);
        fflush(stdout);
        return;
    }
    ierr = fastbit_iapi_register_array("a2", FastBitDataTypeBitRaw, a2, n);
    if (ierr < 0) {
        printf("Warning -- fastbit_iapi_register_array failed to register a2, "
               "ierr = %ld\n", ierr);
        fflush(stdout);
        return;
    }
    ierr = fastbit_iapi_register_array_ext
        ("a3", FastBitDataTypeDouble, &(dbl.n), 1, &dbl, mydblreader);
    if (ierr < 0) {
        printf("Warning -- fastbit_iapi_register_array failed to register a3, "
               "ierr = %ld\n", ierr);
        fflush(stdout);
        return;
    }

    /* build index on a3, automatically attached */
    ierr = fastbit_iapi_build_index("a3", "<binning nbins=10/>");
    if (ierr < 0) {
        printf("Warning -- fastbit_iapi_build_index failed to create index "
               "for a3, ierr=%ld\n", ierr);
        fflush(stdout);
    }

    { /* Serialize the index for a3 and register the index alone as a3 */
        uint64_t nk, nf, nb, nv=n;
        ierr = fastbit_iapi_deconstruct_index
            ("a3", &keys3, &nk, &offsets3, &nf, &bms3, &nb);
        if (ierr >= 0) {
            if (msglvl > 3) {
                fflush(stdout);
                printf("fastbit_iapi_deconstruct_index returned nk=%lld, "
                       "nf=%lld, and nb=%lld\n", nk, nf, nb);
                if (nk==2*(nf-1)) { // binned index
                    for (i = 0; i < nf-1; ++ i) {
                        printf("[%lg, %lg) [%lli, %lli)\n", keys3[i],
                               keys3[nf-1+i], offsets3[i], offsets3[i+1]);
                    }
                }
                else { // unbinned
                    for (i = 0; i < nf-1; ++ i) {
                        printf("[%lg] [%lli, %lli)\n", keys3[i],
                               offsets3[i], offsets3[i+1]);
                    }
                }
                fflush(stdout);
            }

            fastbit_iapi_free_array("a3");
            ierr = fastbit_iapi_register_array_index_only
                ("a3", FastBitDataTypeShort, &nv, 1,
                 keys3, nk, offsets3, nf, bms3, mybmreader);
            if (ierr < 0) {
                fflush(stdout);
                printf("Warning -- fastbit_iapi_attach_index failed "
                       "to attach the index to a3, ierr = %ld\n", ierr);
                fflush(stdout);
            }
        }
        else {
            fflush(stdout);
            printf("Warning -- fastbit_iapi_deconstruct_index failed "
                   "to serialize the index of a3, ierr = %ld\n", ierr);
            fflush(stdout);
        }
    }

    /* a1 < b1 */
    h1 = fastbit_selection_osr("a1", FastBitCompareLess, b1);
    /* a2 <= b2 */
    h2 = fastbit_selection_osr("a2", FastBitCompareLessEqual, b2);
    /* b31 <= a3 < b32 */
    h3 = fastbit_selection_combine
        (fastbit_selection_osr("a3", FastBitCompareGreaterEqual, b31),
         FastBitCombineAnd,
         fastbit_selection_osr("a3", FastBitCompareLess, b32));
    /* a1 < b1 AND b31 <= a3 < b32 */
    h4 = fastbit_selection_combine(h1, FastBitCombineAnd, h3);
    /* a2 <= b2 AND (a1 < b1 AND b31 <= a3 < b32) */
    h5 = fastbit_selection_combine(h2, FastBitCombineAnd, h4);

    ierr = fastbit_selection_evaluate(h5);
    fflush(stdout);
    if (ierr < 0) {
        printf("Warning -- fastbit_selection_evaluate(...) returned %ld\n",
               ierr);
    }
    else {
        long int expected = (n / 2);
        if (ierr != expected) {
            printf("Warning -- fastbit_selection_evaluate(...) expected %ld, "
                   "but got %ld\n", expected, ierr);
        }
        else {
            printf("fastbit_selection_evaluate(...) returned %ld as expected\n",
                   ierr);
        }
    }
    fflush(stdout);
    /* free cached results, not h5 itself */
    fastbit_selection_purge_results(h5);

    /* double the data */
    fflush(stdout);
    printf("\n*** queryarray2 - will duplicate the data ***\n");
    fflush(stdout);
    ierr = fastbit_iapi_extend_array("a1", FastBitDataTypeBitRaw, a1, n);
    if (ierr < 0) {
        printf("Warning -- fastbit_iapi_extend_array failed to register a1, "
               "ierr = %ld\n", ierr);
        fflush(stdout);
    }
    ierr = fastbit_iapi_extend_array("a2", FastBitDataTypeBitRaw, a2, n);
    if (ierr < 0) {
        printf("Warning -- fastbit_iapi_extend_array failed to register a2, "
               "ierr = %ld\n", ierr);
        fflush(stdout);
    }
    ierr = fastbit_iapi_extend_array("a3", FastBitDataTypeDouble, a3, n);
    if (ierr < 0) {
        printf("Warning -- fastbit_iapi_extend_array failed to register a3, "
               "ierr = %ld\n", ierr);
        fflush(stdout);
    }

    ierr = fastbit_selection_evaluate(h5);
    fflush(stdout);
    if (ierr < 0) {
        printf("Warning -- fastbit_selection_evaluate(...) returned %ld\n",
               ierr);
    }
    else {
        long int expected = n;
        if (ierr != expected) {
            printf("Warning -- fastbit_selection_evaluate(...) expected %ld, "
                   "but got %ld\n", expected, ierr);
        }
        else {
            printf("fastbit_selection_evaluate(...) returned %ld as expected\n",
                   ierr);
        }
    }
    fflush(stdout);

    fastbit_selection_free(h5);
    fastbit_iapi_free_all();
    free(offsets3);
    free(keys3);
    free(bms3);
} /* queryarray2 */

int main(int argc, char **argv) {
    long int k, nmax=1000;
    int ierr;
    const char *conffile=0;
    int16_t *a1;
    int32_t *a2;
    double  *a3;
    if (argc > 1) {
        double tmp;
        ierr = sscanf(argv[1], "%lf", &tmp);
        if (ierr == 1) {
            nmax = tmp;
        }
    }
    if (argc > 2) {
        ierr = sscanf(argv[2], "%d", &msglvl);
        if (ierr <= 0)
            msglvl = 0;
    }
    if (argc > 3)
        conffile = argv[3];
#if defined(DEBUG) || defined(_DEBUG)
#if DEBUG + 0 > 10 || _DEBUG + 0 > 10
    msglvl = INT_MAX;
#elif DEBUG + 0 > 0
    msglvl += 7 * DEBUG;
#elif _DEBUG + 0 > 0
    msglvl += 5 * _DEBUG;
#else
    msglvl += 3;
#endif
#endif
    msglvl += 1;
    a1 = (int16_t*)malloc(2*nmax);
    a2 = (int32_t*)malloc(4*nmax);
    a3 = (double*)malloc(8*nmax);
    fastbit_init(conffile);
    fastbit_set_verbose_level(msglvl);
    for (k = 1; k <= nmax; k=((k>(nmax/4)&&k<nmax) ? nmax : 4*k)) {
        printf("\n%s -- testing with k = %ld\n", *argv, k);
        fillarray2(k, a1, a2, a3);
        queryarray2(k, a1, a2, a3);

        /*
        fillarrays(k, a1, a2, a3);
        queryarrays(k, a1, a2, a3);
        */
        // need to clear all cached objects so that we can reuse the same
        // pointers a1, a2, a3
        fastbit_iapi_free_all();
    }
    fastbit_cleanup();
    free(a3);
    free(a2);
    free(a1);
    return 0;
} /* main */

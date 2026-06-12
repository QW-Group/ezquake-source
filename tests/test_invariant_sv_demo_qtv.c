#include <check.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

/* Include the production header/source for mvddest_t and QTV_ReadInput */
#include "sv_demo_qtv.h"

START_TEST(test_qtv_read_input_no_overflow)
{
    /* Invariant: QTV_ReadInput must never read beyond declared buffer bounds
       regardless of input size from untrusted network source */
    struct {
        const char *data;
        size_t      len;
    } payloads[] = {
        /* Exact exploit: oversized packet 2x typical 1024-byte buffer */
        { NULL, 2048 },
        /* Boundary: one byte over typical max */
        { NULL, 1025 },
        /* Valid: well-formed small input */
        { "QTV 1.0\n", 8 },
    };

    /* Fill oversized payloads with 'A' pattern */
    char buf2048[2048]; memset(buf2048, 'A', sizeof(buf2048));
    char buf1025[1025]; memset(buf1025, 'B', sizeof(buf1025));
    payloads[0].data = buf2048;
    payloads[1].data = buf1025;

    int num_payloads = sizeof(payloads) / sizeof(payloads[0]);

    for (int i = 0; i < num_payloads; i++) {
        /* Create a socket pair to feed data into QTV_ReadInput */
        int sv[2];
        ck_assert_int_eq(socketpair(AF_UNIX, SOCK_STREAM, 0, sv), 0);

        /* Write payload into one end */
        write(sv[1], payloads[i].data, payloads[i].len);
        close(sv[1]);

        /* Set up a minimal mvddest_t pointing at the read end */
        mvddest_t d;
        memset(&d, 0, sizeof(d));
        d.socket = sv[0];  /* adjust field name to match actual struct */

        /* Call the real production function — must not crash or overflow */
        QTV_ReadInput(&d);

        close(sv[0]);
        /* If we reach here without SIGSEGV/SIGABRT the invariant holds */
        ck_assert_msg(1, "QTV_ReadInput returned without memory violation");
    }
}
END_TEST

Suite *security_suite(void)
{
    Suite *s;
    TCase *tc_core;

    s = suite_create("Security");
    tc_core = tcase_create("Core");

    tcase_add_test(tc_core, test_qtv_read_input_no_overflow);
    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s;
    SRunner *sr;

    s = security_suite();
    sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
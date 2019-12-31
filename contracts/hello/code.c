#include <ourcontract.h>
//#include <string>

static struct {
    int is_freezed;
    int user_count;
} state;

static void state_init()
{
    state.is_freezed = 0;
    state.user_count = 0;
    out_clear();
}

int contract_main(int argc, char **argv)
{
    if (state_read(&state, sizeof(state)) == -1) {
        /* first time call */
        state_init();
        state_write(&state, sizeof(state));
        state_read(&state, sizeof(state));
    }
/*
    if (argc < 2) {
        std::string s = "Hello World!\n";
        err_printf("%s", s);
    }
*/
    err_printf("%s: Hello, world!\n", argv[0]);
    return 0;
}

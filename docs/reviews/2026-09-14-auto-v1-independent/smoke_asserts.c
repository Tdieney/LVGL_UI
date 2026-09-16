#include <windows.h>
#include <stdlib.h>
#define main submitted_smoke_main
#include "../../../sim_pc/smoke_production.c"
#undef main
int main(void)
{
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    _set_error_mode(_OUT_TO_STDERR);
    return submitted_smoke_main();
}

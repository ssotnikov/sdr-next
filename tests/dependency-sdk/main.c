#include <SoapySDR/Version.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    const char *abi = SoapySDR_getABIVersion();
    const char *version = SoapySDR_getLibVersion();
    if (!abi || !version)
        return 1;
    printf("SoapySDR %s; ABI %s\n", version, abi);
    return strcmp(abi, SOAPY_SDR_ABI_VERSION) != 0 ||
           (strcmp(version, "0.8.1") != 0 && strncmp(version, "0.8.1-", 6) != 0);
}

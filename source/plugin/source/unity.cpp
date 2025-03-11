#include "../headers/includes.hpp"

jbc_cred g_Creds, g_RootCreds;

static bool nativeDialogInitialized = false;
static bool hasBrokenFromTheSandbox = false;

enum Commands : int
{
    INVALID_CMD = -1,
    ACTIVE_CMD = 0,
    LAUNCH_CMD,
    PROCLIST_CMD,
    KILL_CMD,
    KILL_APP_CMD,
    JAILBREAK_CMD
};

struct HijackerCommand
{
    int magic = 0xDEADBEEF;
    Commands cmd = INVALID_CMD;
    int PID = -1;
    int ret = -1337;
    char msg1[0x500];
    char msg2[0x500];
};

void InitializeNativeDialogs()
{
    if (!nativeDialogInitialized)
    {
        nativeDialogInitialized = true;

        bool wasAlreadyFree = false;

        if (IsFreeOfSandbox())
            wasAlreadyFree = true;

        BreakFromSandbox();

        printAndLog(1, "Initiating native dialogs...");

        sceSysmoduleLoadModule(ORBIS_SYSMODULE_MESSAGE_DIALOG);
        sceCommonDialogInitialize();
        sceMsgDialogInitialize();

        sceKernelLoadStartModule("/system/common/lib/libSceAppInstUtil.sprx", 0, NULL, 0, NULL, NULL);
        sceKernelLoadStartModule("/system/common/lib/libSceBgft.sprx", 0, NULL, 0, NULL, NULL);

        if (!wasAlreadyFree)
            EnterSandbox();
    }
}

bool IsFreeOfSandbox()
{
    if (hasBrokenFromTheSandbox)
        return true;

    if (IsPlayStation5())
    {
        struct stat info;
        if (stat("/data/etaHEN/", &info) != 0)
            return false;

        return (info.st_mode & S_IFDIR) != 0;
    }

    FILE *filePtr = fopen("/user/.jailbreak", "w");
    if (!filePtr)
        return false;

    fclose(filePtr);
    
    return (remove("/user/.jailbreak") == 0);
}

void EnterSandbox()
{
    if (IsPlayStation5())
        return;

    if (IsFreeOfSandbox())
        jbc_set_cred(&g_Creds);
}

void BreakFromSandbox()
{
    if (IsFreeOfSandbox())
        return;

    if (IsPlayStation5())
    {
        struct sockaddr_in address;
        address.sin_len = sizeof(address);
        address.sin_family = AF_INET;
        address.sin_port = htons(9028);
        memset(&(address.sin_zero), 0, sizeof(address.sin_zero));
        inet_pton(AF_INET, "127.0.0.1", &(address.sin_addr));

        int sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0)
        {
            perror("Socket creation failed");
            puts("Jailbreak failed");
            return;
        }

        if (connect(sock, (struct sockaddr *)&address, sizeof(address)) < 0)
        {
            perror("Failed to connect to daemon");
            close(sock);
            puts("Jailbreak failed");
            return;
        }

        HijackerCommand cmd;
        cmd.PID = getpid();
        cmd.cmd = JAILBREAK_CMD;

        if (send(sock, (void *)&cmd, sizeof(cmd), MSG_NOSIGNAL) == -1)
        {
            puts("Failed to send command");
            close(sock);
            puts("Jailbreak failed");
            return;
        }

        if (recv(sock, reinterpret_cast<void *>(&cmd), sizeof(cmd), MSG_NOSIGNAL) == -1)
        {
            puts("Failed to receive response from daemon");
            close(sock);
            puts("Jailbreak failed");
            return;
        }

        close(sock);

        if (cmd.ret != 0 && cmd.ret != -1337)
        {
            puts("Jailbreak has failed");
            return;
        }
    }
    else
    {
        jbc_get_cred(&g_Creds);
        g_RootCreds = g_Creds;
        jbc_jailbreak_cred(&g_RootCreds);
        jbc_set_cred(&g_RootCreds);
    }

    hasBrokenFromTheSandbox = true;

}

void MountInSandbox(const char *systemPath, const char *mountName)
{
    if (IsPlayStation5())
        return;

    if (jbc_mount_in_sandbox(systemPath, mountName) == 0)
        PrintToConsole("Mount: Passed", 0);
    else
        PrintToConsole("Mount: Failed", 2);
}

void UnmountFromSandbox(const char *mountName)
{
    if (IsPlayStation5())
        return;

    if (jbc_unmount_in_sandbox(mountName) == 0)
        PrintToConsole("Unmount: Passed", 0);
    else
        PrintToConsole("Unmount: Failed", 2);
}

void ExitApplication()
{
    EnterSandbox();

    sceSystemServiceLoadExec("exit", 0);
}

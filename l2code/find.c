#include <fcntl.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <dirent.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>

#define DEFAULT_FLAG (O_WRONLY | O_CREAT | O_TRUNC)
#define DEFAULT_PERMISSIONS 0644
#define kMaxPath 200


static void listMatches(char path[], size_t length, const char *name)
{
    DIR *dir = opendir(path);
    if(dir == NULL) return;
    strncat(path, "/", 2);
    while(true)
    {
        struct dirent *de = readdir(dir);
        if(de == NULL) break;
        if(strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0) continue;
        if(length + strlen(de->d_name) > kMaxPath) continue;
        strcat(path, de->d_name);
        struct stat st;
        lstat(path, &st);
        if(S_ISREG(st.st_mode))
        {
            if(strcmp(de->d_name, name) == 0) printf("%s\n", path);
        }
        else if (S_ISDIR(st.st_mode))
        {
            listMatches(path, length + strlen(de->d_name), name);
        }
    }
    closedir(dir);
}

int main (int argc, char *argv[])
{
    assert(argc == 3);
    const char *directory = argv[1];
    struct stat st;
    lstat(directory, &st);
    assert(S_ISDIR(st.st_mode));
    size_t length = strlen(directory);
    if(length > kMaxPath) return 0;
    const char *pattern = argv[2];
    char path[kMaxPath + 1];
    strcpy(path, directory);
    listMatches(path, length, pattern);
    return 0;
}
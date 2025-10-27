#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))

void usage(char *progname)
{
    fprintf(stderr, "USAGE: %s [-p <arg>] [-c <arg>]\n", progname);
    //exit(EXIT_FAILURE);
}
void scan_dir(char * path, int fd)
{
    DIR *dirp;
    struct dirent *dp;
    struct stat filestat;
    if((dirp=opendir(path))==NULL)
        fprintf(stderr,"Cannot open directory %s\n",path);
    else
    {
        printf("Contents of directory %s:\n",path);
        do{
            errno=0;
            if((dp=readdir(dirp))!=NULL){
                if(lstat(dp->d_name,&filestat))
                    ERR("lstat");
                if(dp!= NULL&&strcmp(dp->d_name,".")&&strcmp(dp->d_name,".."))
                {
                    char buf[1024];
                    chdir(path);
                    ssize_t s = snprintf(NULL, 0, "%s %ld\n", dp->d_name,filestat.st_size);
                    snprintf(buf, s+1, "%s %ld\n", dp->d_name, filestat.st_size);
                    write(fd, buf, strlen(buf));
                }
            }
        }while(dp!=NULL);
        if(errno!=0)
            ERR("readdir");
        if(closedir(dirp))
            ERR("closedir");
    }
    printf("\n");
}
int main(int argc, char **argv)
{
    int c;
    char **plist = NULL;
    int pcount = 0;
    char *config = NULL;
    while ((c = getopt(argc, argv, "p:c:")) != -1) {
        switch (c) {
            case 'p':
                plist = realloc(plist, (pcount + 1) * sizeof(char *));
                plist[pcount++] = optarg;
                break;
            case 'c':
                config = optarg;
                break;
            default:
                 usage(argv[0]);
                return EXIT_FAILURE;
        }
    }
    int i;
    int fd=open(config, O_CREAT|O_WRONLY,0666);
    for (i = 0; i < pcount; i++) {
        printf("Path %d: %s\n", i + 1, plist[i]);
        write(fd,"\n",1);
        scan_dir(plist[i], fd);
    }
    if (config) {
        printf("Config: %s\n", config);
    }
    free(plist);
    return EXIT_SUCCESS;
}
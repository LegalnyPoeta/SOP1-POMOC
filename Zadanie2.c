#define _XOPEN_SOURCE 700
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <ftw.h>
#include <stdbool.h>

#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))
bool O_flag = false;
int MAXFD =1;
char *ext = NULL;

void usage(char *progname)
{
    fprintf(stderr, "USAGE: -<p|d|e|o> [<value>]\n"
                    "\t-p\tspecify directory names (multiple allowed)\n"
                    "\t-d\tdepth of scanning (default 1)\n"
                    "\t-e\tfile extension to look for(only one allowed)\n"
                    "\t-o\tSave results to a file\n");
    exit(EXIT_FAILURE);
}

int walk(const char *name, const struct stat *s, int type, struct FTW *f)
{
    if (f->level > MAXFD)
    {
        return 0;
    }    
    FILE *output=stdout;
    if (O_flag)
    {
        char *path = getenv("L1_OUTPUTFILE");
        if(path!=NULL)
        {
            output = fopen(path, "a");
        }
    }
    if (type == FTW_F && (ext == NULL || strcmp(name+strlen(name)-strlen(ext), ext)==0))
        fprintf(output,"%s %ld\n", name, s->st_size);
    if (O_flag&& output!=stdout)
        fclose(output);
    return 0;
}

void visit_dir(char *path, char *ext, bool O_flag, int i)
{
    FILE *output=stdout;
    if (O_flag)
    {
        char *path = getenv("L1_OUTPUTFILE");
        if(path!=NULL)
        {
            if(i==0)
                output = fopen(path, "w+");
            else
                output = fopen(path, "a");
        }
    }
    fprintf(output,"\nVisiting directory: %s\n", path);
    if (O_flag&& output!=stdout)
        fclose(output);
    if (nftw(path, walk, 20, FTW_DEPTH) == -1)
        ERR("nftw");
}

void path(int argc, char **argv)
{
    bool P_flag = false;
    int depth = 1;
    char **dir_names = NULL;
    int i=0,j=0,c;
    while((c = getopt(argc, argv, "p:d:e:o")) != -1)
    {
        switch(c)
        {
            case 'p':
                P_flag=true;
                dir_names = realloc(dir_names, (sizeof(char*) * (i + 1)));
                dir_names[i]=optarg;
                i++;
                break;
            case 'd':
                if(!P_flag)
                    usage(argv[0]);
                MAXFD=atoi(optarg);
                if(MAXFD<1)
                    usage(argv[0]);
                break;
            case 'e':
                if(!P_flag||ext!=NULL)
                    usage(argv[0]);
                ext = malloc(strlen(optarg) + 1);
                strcpy(ext, optarg);
                printf("Extension: %s\n", ext);
                break;
            case 'o':
                if(!P_flag)
                    usage(argv[0]);
                O_flag=true;
                break;
            case '?':
            default:
                usage(argv[0]);
        }
    }
    if(!P_flag)
        usage(argv[0]);
    printf("Depth: %d\n", depth);
    for(j=0; j<i;j++)
        visit_dir(dir_names[j], ext, O_flag,j);
    free(dir_names);
}

int main(int argc, char **argv)
{
    path(argc, argv);
    return EXIT_SUCCESS;
}
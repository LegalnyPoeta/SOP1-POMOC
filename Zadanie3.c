#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdbool.h>

#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))

void usage(char *progname)
{
    fprintf(stderr, "USAGE: -<c|v|i|r> [<value>]\n"
                    "\t-c\tcreate new environment\n"
                    "\t-v\tenv name\n"
                    "\t-i\tinstall package from requirements file(one allowed)\n"
                    "\t-r\tremove package from environment(one allowed)\n");
    exit(EXIT_FAILURE);
}

void add_pack(char *dir, char* name, char* version)
{
    DIR *dirp;
    FILE *f1,*f2;

    if((dirp=opendir(dir))==NULL)
        ERR("opendir");
    chdir(dir);
    
    if(access(name, F_OK)!=-1)
    {
        printf("package %s already installed in %s\n", name, dir);
        chdir("..");
        if(closedir(dirp)) ERR("closedir");
        return;
    }

    if((f1=fopen(name,"w+"))==NULL)
        ERR("fopen");
    for(int i=0;i<1000;i++)
        fputc(rand()%127, f1);
    fclose(f1);

    if((f2=fopen("requirements", "a"))==NULL)
        ERR("fopen");
    fprintf(f2, "%s %s\n", name, version);
    fclose(f2);

    chdir("..");
    if(closedir(dirp))
        ERR("dirp");
    printf("package %s version %s installed in %s\n", name, version, dir);
}

void remove_pack(char *env, char* name)
{
    FILE *f1,*f2;
    char line[256];
    chdir(env);
    if((f1=fopen(name, "r"))==NULL)
    {
        printf("package %s is not installed in %s so it cannot be removed\n", name, env);
        return;
    }
    remove(name);
    fclose(f1);

    if((f1=fopen("requirements", "r"))==NULL)
        ERR("fopen");
    if((f2=fopen("temp","w"))==NULL)
        ERR("fopen");
    
    while(fgets(line, sizeof(line), f1))
    {
        if(strstr(line, name)==NULL)
            fprintf(f2,"%s\n",line);
    }
    remove("requirements");
    fclose(f1);
    rename("temp", "requirements");
    fclose(f2);
    printf("Sucessfully removed package %s from %s\n", name, env);
    chdir("..");
}

void prepare_pack(char *env, char* pack, bool remove, bool add)
{
    char *pack_copy=strdup(pack);
    char *name=strtok(pack_copy, "==");
    char *version=strtok(NULL, "==");
    if(version==NULL)
        version="1.0";
    if(add) add_pack(env, name, version);
    if(remove) remove_pack(env, name);
    free(pack_copy);
}

void create_env(char * path)
{
    errno=0;
    DIR *dirp;
    if((dirp=opendir(path))==NULL)
    {
        if(errno==ENOENT)
        {
            mkdir(path,0611);
            chdir(path);
            fclose(fopen("requirements", "w+"));
            chdir("..");
            printf("env created\n");
        }
        else    
            ERR("opendir");
    }
    else
    {
        printf("env already exists\n");
        if(closedir(dirp))
            ERR("closedir");
    }
}

void path(int argc, char **argv)
{
    bool C_flag =false;
    char **v_value = NULL;
    char *i_value = NULL;
    char *r_value = NULL;
    int i=0,c;
    while((c = getopt(argc, argv, "cv:i:r:")) != -1)
    {
        switch(c)
        {
            case 'c':
                C_flag=true;
                break;
            case 'v':
                v_value = realloc(v_value, (sizeof(char*) * (i + 1)));
                v_value[i]=optarg;
                i++;
                if(C_flag&& i==2)
                    usage(argv[0]);
                break;
            case 'i':
                if(i_value!=NULL)
                    usage(argv[0]);
                i_value=optarg;
                break;
            case 'r':
                if(r_value!=NULL)
                    usage(argv[0]);
                r_value=optarg;
                break;
            case '?':
            default:
                printf("c: %d\n", c);
                printf("case default: %s\n", optarg);
                usage(argv[0]);
        }
    }
    if(v_value==NULL)
        usage(argv[0]);
    if(C_flag)
        create_env(v_value[0]);
    if(i_value!=NULL)
    {
        int j=0;
        for(j=0;j<i;j++)
        {
            prepare_pack(v_value[j], i_value, false, true);
        }
    }
    if(r_value!=NULL)
    {
        int j=0;
        for(j=0;j<i;j++)
        {
            prepare_pack(v_value[j], r_value, true, false);
        }
    }
    free(v_value);
    if(r_value==NULL && i_value==NULL && !C_flag)
        usage(argv[0]);
}

int main(int argc, char **argv)
{
    path(argc, argv);
    return EXIT_SUCCESS;
}
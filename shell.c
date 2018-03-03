/******************************************************************************************
*
* Antoine Louis & Tom Crasset
*
* Operating systems : Projet 2 - Shell with built-ins
*******************************************************************************************/

#include <sys/types.h> 
#include <sys/wait.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>



/*************************************Prototypes*********************************************/
int split_line(char* line, char** args);
int get_paths(char** paths);
void convert_whitespace_dir(char** args);



/*************************************split_line*****************************************
*
* Split the line line entered by the user
*
* ARGUMENT :
*   - line : a string entered by the user as a line line
*
* RETURN : an array of strings reprensenting each token entered by the user
*
*******************************************************************************************/
int split_line(char* line, char** args){

    int nb_args = 0;
    char* token;
    
    token = strtok(line, "\n");
    token = strtok(line, " ");

    while(token != NULL){

        args[nb_args] = token;
        nb_args++;        
        token = strtok(NULL, " ");
    }

    args[nb_args] = (char*) NULL;

    return nb_args;

}


/*************************************get_paths****************************************
*
* Split the full path into all possible paths and get the number of total paths
*
* ARGUMENT :
*   - paths : an array that will contain all the paths
*
* RETURN : the number of paths
*
*******************************************************************************************/
int get_paths(char** paths) {

    char* pathstring = getenv("PATH"); //get the $PATH environment variable
    int nb_paths = 0;

    char* path = strtok(pathstring,":"); //Parse the string for a path delimited by ":"

    while(path != NULL){
        paths[nb_paths] = path;
        nb_paths++;
        path = strtok(NULL,":"); //Parse the array for the next path delimited by ":"
    }

    return nb_paths;
}



/*************************************convert_whitespace_dir************************************
*
* Convert a directory/folder with special characters to a directory/folder with whitespaces
*
* ARGUMENT :
*   - args : an array containing all the args of the line  entered by the user
*
* NB: it will clear all args except args[0] and args[1]
*
*******************************************************************************************/
void convert_whitespace_dir(char** args){

    int j=1;
    char path[256];
    strcpy(path,"");
    char* token;
    char delimiters[] = "\"\'\\";


    while(args[j] != NULL){

        //Get the first token delimited by one of the delimiters
        token = strtok(args[j], delimiters);


        while(token != NULL){
            //Add this token to the path
            strcat(path, token);
            //Get the next token
            token = strtok(NULL, delimiters);
        }

        //Add a whitespace
        strcat(path, " ");

        j++;
    }

    //Removing the last whitespace
    path[strlen(path)-1] = 0;

    //Cleaning all arguments except cmd and directory (args[0] and args[1])
    memset(&args[2], 0, sizeof(args)-2);
    
    //Copy the path to the unique argument of cd
    strcpy(args[1], path);
}



/*************************************find_in_file*****************************************
*
* Find a certain string in a file 
*
* ARGUMENT :
*   - path : the corresponding path of the file
*   - searched_str : the searched string in the file
*   - output_str : the corresponding output to the searched string
*   - number : 
*
* RETURN : true if the string has been found, false otherwise.
*
*******************************************************************************************/
bool find_in_file(const char* path, char* searched_str, char** output_str, int number){

    FILE* file;
    char* line = NULL;
    size_t len = 0;
    bool result = false;

    //Opening the file
    file = fopen(path, "r");
    if(file == NULL){
        perror("File couldn't be opened");
        printf("1");
        return false;
    }

    
    while(getline(&line, &len, file) != -1){

        if(!strcmp(searched_str, "hostname")){
            *output_str = line;
            fclose(file);
            return true;
        }

        if(strstr(line, searched_str)){
            
            if(number != 0){
                number--;
                continue;
            }

            *output_str = strtok(line,":");
            *output_str = strtok(NULL, "");
            result = true;
            break;
        }

    }

    fclose(file);

    return result;
}






/******************************************main**********************************************/
int main(int argc, char** argv){

    bool stop = false;
    int returnvalue;

    char line[65536]; 
    char* args[256];
    
    pid_t pid;
    int status;
    
    char* output_str = NULL;

    while(!stop){

        //Clear the variables
        strcpy(line,"");
        memset(args, 0, sizeof(args));

        //Prompt
        printf("> ");
        fflush(stdout);

        //User wants to quit (using Ctrl+D or exit())
        if(fgets(line,sizeof(line),stdin) == NULL || !strcmp(line,"exit\n")){
            stop = true;
            break;
        }

        //User presses "Enter"
        if(!strcmp(line,"\n"))
            continue;

        //User enters a line 
        int nb_args = split_line(line, args);

        //The command is cd
        if(!strcmp(args[0], "cd")){

            // Case 1 : cd or cd ~
            if(args[1] == NULL || !strcmp(args[1],"~"))
                args[1] = getenv("HOME");

            //Case 2 : cd ..
            else if(!strcmp(args[1],"..")){

                char* new_dir = strrchr(args[1],'/');

                if(new_dir != NULL)
                    *new_dir = '\0';
            }


            /*Case 3 :  cd FirstDir/"My directory"/DestDir
                        cd FirstDir/'My directory'/DestDir
                        cd FirstDir/My\ directory/DestDir
            */
            if(nb_args > 2){ //Means that there is/are (a) folder(s) with whitespace

                convert_whitespace_dir(args);
            }

            
            printf("\n%d",chdir(args[1]));
            continue;
        }      

        if(!strcmp(args[0], "sys")){


            //Gives the hostname without using a system call
            if ((args[1]!=NULL)&&(!strcmp(args[1], "hostname"))){

                if(!find_in_file("/proc/sys/kernel/hostname", "hostname", &output_str, 0)){
                    perror("No such string found");
                    printf("1");
                    continue;
                }

                printf("%s0", output_str);

                continue;

            }


            //Gives the CPU model
            if ((args[1]!=NULL)&&(args[2]!=NULL)&&
                (!strcmp(args[1], "cpu"))&&(!strcmp(args[2], "model"))){

                if(!find_in_file("/proc/cpuinfo", "model name", &output_str, 0)){
                    perror("No such string found");
                    printf("1");
                    continue;
                }

                printf("%s0", output_str);

                continue;
            }


            //Gives the CPU frequency of Nth processor
            if ((args[1]!=NULL)&&(args[2]!=NULL)&&
                (!strcmp(args[1], "cpu"))&&(!strcmp(args[2], "freq"))&&
                (args[3]!= NULL)&&(args[4]==NULL)){

                if(!find_in_file("/proc/cpuinfo", "cpu MHz", &output_str, atoi(args[3]))){
                    perror("No such string found");
                    printf("1");
                    continue;
                }

                printf("%s0", output_str);
                continue;


            }


            //Set the frequency of the CPU N to X (in HZ)
            else if ((args[1]!=NULL)&&
                    (args[2]!=NULL)&&
                    (!strcmp(args[1], "cpu"))&&
                    (!strcmp(args[2], "freq"))&&
                    (args[3]!= NULL)&&
                    (args[4]!=NULL)){

                int number = atoi(args[3]);
                char path[256];
                size_t frequency = atoi(args[4]);
                snprintf(path,sizeof(number),"/sys/devices/system/cpu/cpu%d/cpufreq/scaling_cur_freq",number);



            }


            //Get the ip and mask of the interface DEV
            else if ((args[1] != NULL)&&
                    (args[2] != NULL)&&
                    (!strcmp(args[1], "ip"))&&
                    (!strcmp(args[2], "addr"))&&
                    (args[3] != NULL)&&
                    (args[4] == NULL)){

                    char* dev = args[3];

                    // Create a socket in UDP mode
                    int socket_desc = socket(AF_INET , SOCK_DGRAM , 0);
     
                    if (socket_desc == -1){
                        printf("Socket couldn't be created\n");
                        printf("1");
                        continue;
                    }


                    //Creating an interface structure
                    struct ifreq my_ifreq; 

                    size_t length_if_name= strlen(dev);
                    //Check that the ifr_name is big enough
                    if (length_if_name < IFNAMSIZ){ 

                        memcpy(my_ifreq.ifr_name,dev,length_if_name);
                        my_ifreq.ifr_name[length_if_name]=0; //End the name with terminating char
                    }else{

                        perror("The interface name is too long");
                        continue;
                    }

                    // Get the IP address, if successful, adress is in  my_ifreq.ifr_addr
                    if(ioctl(socket_desc,SIOCGIFADDR,&my_ifreq) == -1){


                        perror("Couldn't retrieve the IP address");
                        close(socket_desc);
                        printf("1");
                        continue;
                    }

                    //Extract the address
                    struct sockaddr_in* IP_address = (struct sockaddr_in*) &my_ifreq.ifr_addr;
                    printf("IP address: %s\n",inet_ntoa(IP_address->sin_addr));

                    // Get the mask, if successful, mask is in my_ifreq.ifr_netmask
                    if(ioctl(socket_desc, SIOCGIFNETMASK, &my_ifreq) == -1){

                        perror("Couldn't retrieve the mask");
                        close(socket_desc);
                        printf("1");
                        continue;
                    }

                    //Cast and extract the mask
                    struct sockaddr_in* mask = (struct sockaddr_in*) &my_ifreq.ifr_addr;
                    printf("Mask: %s\n",inet_ntoa(mask->sin_addr));

                    close(socket_desc);

            }

        
            //Set the ip of the interface DEV to IP/MASK
            else if ((args[1]!=NULL)&&
                (args[2]!=NULL)&&
                (!strcmp(args[1], "ip"))&&
                (!strcmp(args[2], "addr"))&&
                (args[3]!= NULL)&&
                (args[4]!=NULL)&&
                (args[5]!=NULL)){


                //Interface name and length
                char* dev = args[3]; 
                size_t length_if_name= strlen(dev); 

                char* address = args[4];
                char* mask = args[5];

                // Create a socket in UDP mode
                int socket_desc = socket(AF_INET , SOCK_DGRAM , 0);
 
                if (socket_desc == -1){
                    perror("Socket couldn't be created\n");
                    printf("1");
                    continue;
                }

                //Creating an interface structure
                struct ifreq my_ifreq; 

                //Check that the ifr_name is big enough
                if (length_if_name < IFNAMSIZ){ 
                    //Set the name of the interface you want to look at
                    memcpy(my_ifreq.ifr_name,dev,length_if_name);
                    //End the name with terminating char
                    my_ifreq.ifr_name[length_if_name]=0;

                }else{
                    close(socket_desc);
                    perror("The interface name is too long");
                    printf("1");
                    continue;
                }

                //Creating an address structure;
                struct sockaddr_in* address_struct = (struct sockaddr_in*)&my_ifreq.ifr_addr;
                my_ifreq.ifr_addr.sa_family = AF_INET;

                // Converting from string to address structure
                inet_pton(AF_INET, dev,  &address_struct->sin_addr);

                //Setting the new IP address
                if(ioctl(socket_desc, SIOCSIFADDR, &my_ifreq) == -1){

                    perror("Couldn't set the address");
                    printf("1");
                    close(socket_desc);
                    continue;
                }
                // Converting from string to address structure
                inet_pton(AF_INET, mask,  &address_struct->sin_addr);
                //Setting the mask
                if(ioctl(socket_desc, SIOCSIFNETMASK, &my_ifreq) == -1){

                    perror("Couldn't set the mask");
                    printf("1");
                    close(socket_desc);
                    continue;
                }


                ioctl(socket_desc, SIOCGIFFLAGS, &my_ifreq); //Load flags
                my_ifreq.ifr_flags |= IFF_UP | IFF_RUNNING; //Change flags
                ioctl(socket_desc, SIOCSIFFLAGS, &my_ifreq); //Save flags
                close(socket_desc);

            }
            else{
                printf("1");
                continue;
            }
        }  


        //The command isn't a built-in command
        pid = fork();

        //Error
        if(pid < 0){
            int errnum = errno;

            perror("Process creation failed");
            fprintf(stderr, "Value of errno: %d\n",errno);
            fprintf(stderr, "Error: %s \n",strerror(errnum));
            exit(EXIT_FAILURE);
        }

        //This is the son
        if(pid == 0){

            //Absolute path of command
            if(args[0][0] == '/'){
                if(execv(args[0],args) == -1){
                    int errnum = errno;
                    perror("Instruction failed");
                    fprintf(stderr, "Value of errno: %d\n",errno);
                    fprintf(stderr, "Error: %s \n",strerror(errnum));
                }
            }

            //Relative path -- Need to check the $PATH environment variable
            else{

                char* paths[256];              

                int nb_paths = get_paths(paths);

                int j = 1;

                /*In the case of commands like mkdir/rmdir, if the first argument is a directory with whitespaces ("a b", 'a b', a\ b),
                  we need to change this directory in something understandable for the shell*/
                if(nb_args > 2){
                    if(args[1][0] == '\"' || args[1][0] == '\'' || args[1][strlen(args[1])-1] == '\\')
                        convert_whitespace_dir(args);
                }

                //Taking a path from paths[] and concatenating with the command
                for(j = 0; j < nb_paths; j++){
                    char path[256] = "";
                    strcat(path,paths[j]);
                    strcat(path,"/");
                    strcat(path,args[0]);
                
                    //Check if path contains the command to execute
                    if(access(path,X_OK) == 0){
                        if(execv(path,args) == -1){
                            int errnum = errno;
                            perror("Instruction failed");
                            fprintf(stderr, "Value of errno: %d\n",errno);
                            fprintf(stderr, "Error: %s \n",strerror(errnum));
                        }

                        break;
                    }
                }
            }

            exit(EXIT_FAILURE);
        }

        //This is the father
        else{
            wait(&status);
            returnvalue = WEXITSTATUS(status);
            printf("\n%d",returnvalue);
            
        }

    }

    return 0;
}
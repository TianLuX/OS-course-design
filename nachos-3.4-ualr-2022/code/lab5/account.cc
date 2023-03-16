
#include"account.h"

Account::Account(int size)
{
    table = new AccountEntity[size];
    tableSize = size;
}

//----------------------------------------------------------------------
// Account::~Account
// 	De-allocate Account data structure.
//----------------------------------------------------------------------

Account::~Account()
{ 
    delete [] table;
}

void
Account::FetchFrom(OpenFile *file)
{   
    //printf("\n%d\n",tableSize);
    (void) file->ReadAt((char *)table, tableSize * sizeof(AccountEntity), 0);
}


void
Account::WriteBack(OpenFile *file)
{   
    //print();
    (void) file->WriteAt((char *)table, tableSize * sizeof(AccountEntity), 0);
}

int
Account::getGroup(char *name){
       for ( int i=0;i<tableSize;i++){
        if (table[i].use && !strncmp(table[i].name, name, userNameLength))
	    return table[i].group;
        }
        return -1;
}
int
Account::getID(char *name){
       for ( int i=0;i<tableSize;i++){
        if (table[i].use && !strncmp(table[i].name, name, userNameLength))
	    return table[i].ID;
        }
        return -1;
}
bool 
Account::Add(char *name,char *passwd){
    bool isAdd=0;

    for(int i=0;i<tableSize;i++)
    {
        if(!table[i].use){
            table[i].use=true;
            table[i].group=defaultGroup;
            table[i].ID=i;
            strncpy(table[i].name, name, userNameLength); 
            
            strncpy(table[i].password, passwd, passwdLength); 
            isAdd=true;
    
            break;
        }
    } 
 
    return isAdd;
} 

int
Account::getGroupByID(int ID){
    return  table[ID].group;
 }
bool
Account::VerifyUser(char*name ,char *passwd){

        for ( int i=0;i<tableSize;i++){
            printf("name:  %s\t  passwd: %s\n",table[i].name,table[i].password);
            if (table[i].use && !strncmp(table[i].name, name, userNameLength))
	        {
                if(!strncmp(table[i].password, passwd, userNameLength)){
                    return true;
                }else{
                    printf("passwd  err\n");
                    return false;
                }
            }
        }
    printf("\ndidn't find user %s\n",name);
    return false;
}

void
Account::print(){
       for ( int i=0;i<tableSize;i++){
            printf("%d %s \t %s\n",i,table[i].name,table[i].password);
       }
}
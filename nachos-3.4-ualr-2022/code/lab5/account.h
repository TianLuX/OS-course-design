#include "openfile.h"
#include "filehdr.h" 
#include<stdio.h>
#ifndef ACCOUNT_H
#define ACCOUNT_H

#define  defaultGroup  0
#define  userNameLength 9
#define  passwdLength   9
//define account struct 
class AccountEntity{
    public:
    //10B
    char name[userNameLength+1];
    //4B
    int group;
    //10B
    char password[passwdLength+1];
    //userID;
    int ID;
    //1B
    bool use;
};

class Account{
 
    public:
     Account(int size); 		
    ~Account();			
    void FetchFrom(OpenFile *file);  	
    void WriteBack(OpenFile *file);	
	int getID(char*name);				
    int getGroup(char *name);				
    bool Add(char *name,char *passwd);  
    int getGroupByID(int ID);
    //verify user  ,login
    bool VerifyUser(char *name ,char *passwd);
    void print();
 private:
    int tableSize;			
    AccountEntity *table;		
    FileHeader *second;
    int FindUser(char *name);	
};
#endif
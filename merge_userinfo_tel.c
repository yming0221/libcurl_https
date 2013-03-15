#include <stdio.h>

int main()
{
	FILE *userinfo;
	FILE *auth_telephone;
	FILE *user_info_tel;
	int ret;
	userinfo = fopen("userinfo","r");
	auth_telephone = fopen("auth_telephone","r");
	user_info_tel = fopen("user_info_tel","w");
#define MAX_LINE 512
	char username[MAX_LINE];
	char passwd[MAX_LINE];
	long long unsigned tel;

	while(fscanf(userinfo,"%s %s",username,passwd) != EOF)
	{
		if(fscanf(auth_telephone,"%llu",&tel) == EOF)
			tel = 0;
		
		fprintf(user_info_tel,"%s %s %llu\n",username,passwd,tel);

	}
	fclose(user_info_tel);
	fclose(auth_telephone);
	fclose(userinfo);

	return 0;
}

/*
	$Id: parser.c,v 1.14 2007-02-16 09:25:44 qqshka Exp $
*/


#define SRCLIMIT 1024
//#define DEBUG 1

#include "quakedef.h"

int Solve_String (char *src);
int Calc_String (char *src,char tok);

// returns -1 on error
// returns 1 on no error
int Remove_Spaces (char *src){
	char msg[1024];

	int j = 0;
	int i = 0;
	if (strlen(src)>SRCLIMIT)
		return -1;
	
	strcpy(msg,src);
	while (msg[i] != '\0' && i < strlen(src)){
		if (msg[i] != ' '){
			src[j++] = msg[i++];
		}else
			i++;

	}
	src[j]='\0';
	return 1;

}


// Checks for unwanted chars in the src string
/*
	returns
	 1: no error
	-1: found an invalid char in src
*/
int Check_Tokens (char *src){
	if (strspn(src,"1234567890-+*/().") == strlen(src))
		return 1;
	else
		return -1;
}

// checks the acmount of ( and ) brackets and returns -1 if they dont match
/* returns
	 x: amount of () brackets
	-1: non matching amounts of ('s and )'s
*/
int Check_Brackets (char *src){
	int count_open,count_close;
	int i;
	count_open = 0;
	count_close = 0;
	for(i=0;i<strlen(src);i++){
		if (src[i] == '(')
			count_open++;
		if (src[i] == ')')
			count_close++;
	}
	if (!(count_close - count_open))
		return count_close;
	else
		return -1;
}


// Checks for double of false tokens ++ -- +- etc
/*
	returns
	 1: no errors
	-1: error while copying the src string
*/
int Check_For_Double_Tokens (char *src){
	char msg[1024];
	int i=0;
	int j=0;
	int found = 1;
	char char_tok[]="+-*/";
	int amount = 2;
	
	if(strlen(src)>SRCLIMIT){
		printf("HALLO %i\n",strlen(src));
		return -1;
	}
	
	while(found){
	i=0;j=0;found=0;
		while(src[i]!='\0' && i<strlen(src)){
			if(strspn(src+i,char_tok)){
				switch (src[i]){
				case '*':
					switch(src[i+1]){
						case '*':
						case '+':
							msg[j++]='*';
							i+=amount;
							found=1;
						break;
						default:
							msg[j++]=src[i++];
					}
					break;
				case '/':
					switch(src[i+1]){
						case '/':
						case '+':
							msg[j++]='/';
							i+=amount;
							found=1;
						break;
						default:
							msg[j++]=src[i++];
					}
					break;
				case '+':
					switch(src[i+1]){
						case '*':
							msg[j++]='*';
							i+=amount;
							found=1;
						break;
						case '+':
							msg[j++]='+';
							i+=amount;
							found=1;
						break;
						case '-':
							msg[j++]='-';
							i+=amount;
							found=1;
						break;
						default:
							msg[j++]=src[i++];
					}
					break;
				case '-':
					switch(src[i+1]){
						case '*':
							msg[j++]='*';
							i+=amount;
							found=1;
						break;
						case '+':
							msg[j++]='-';
							i+=amount;
							found=1;
						break;
						case '-':
							msg[j++]='+';
							i+=amount;
							found=1;
						break;
						default:
							msg[j++]=src[i++];
					}
					break;
				}
			}else{
				msg[j++]=src[i++];
			}
		}
	strcpy(src,msg);
	src[j]='\0';
	}
	j-=1;
	while(src[j]=='*' || src[j]=='+' || src[j]=='-'){
		src[j--]='\0';
	}
		
	j=0;i=0;
	if (src[0] == '*' || src[0] == '+'){
		i++;
		while(src[i]!='\0' && i<strlen(src))
			msg[j++]=src[i++];
	strcpy(src,msg);
	src[j]='\0';
	}
	return 1;
			
}



// works LOL :>
float Calc_AB (char type,float a ,float b){
	switch (type){
		case '*' :
			return a*b;
		case '+' :
			return a+b;
		case '-' :
			return a-b;
		case '/' :
			return a/b;
		default  :
			return -1;
	}
}

// will search the innermost bracket and send the content to Solve_String
/*
	returns
	 1: no error
	-1: error while copying the src string
	-2: error in Check_Brackets
*/
int Solve_Brackets (char *src){
	char msg[1024],part1[1024],part2[1024],bracket[1024];
	int bracket_start,bracket_stop,status;
	int i = 0;
	int y = 0;
	int z = 0;
	#ifdef DEBUG
		printf("Solve Brackets %s\n",src);
	#endif
	if (strlen(src)>SRCLIMIT)
		return -1;
	
	strcpy(msg,src);
	
	status=Check_Brackets(src);
	if (status==-1){
		return -2;
		}
	for (z=0;z<status;z++){
		while(src[i++]!='('){
		}
	}
	bracket_start=i;
	while(src[i] !=')'){
		bracket[y++] = src[i++];
	}
	
	bracket_stop = i;
	bracket[y]='\0';
	
	if(bracket_start){
		strncpy(part1,msg,bracket_start-1);
		part1[bracket_start-1]='\0';
	}
	
	
	if(strlen(msg) != bracket_stop+1){
		strcpy(part2,msg+bracket_stop+1);
	}
	
	Check_For_Double_Tokens(bracket);
	Solve_String(bracket);
	
	if(!bracket_start && strlen(msg) != bracket_stop+1)
		sprintf(src,"%s%s",bracket,part2);
	else if(bracket_start && strlen(msg) == bracket_stop+1)
		sprintf(src,"%s%s",part1,bracket);
	else
		sprintf(src,"%s%s%s",part1,bracket,part2);
	return 1;

}


/*
returns
	0: if there is nothing more to do with the same token,
	1: if there is still smth to do with the same token,
	2: if everything is solved in the sting
   -1: an error while copinyg the src string
*/
int Calc_String (char *src,char tok){
	char msg[1024],part1[1024],part2[1024],a[256],b[256];
	int token_position,count_start,count_stop;
	float fa,fb,val;
	int i=0;
	char char_tok[] 	= "*-+/";
	char num_tok[]		= "1234567890.";
	
	
	// check if src is bigger than msg
	if(strlen(src)>SRCLIMIT)
		return -1;
		
	// Check if src is only 1 number
	if((src[0]=='+' || src[0]=='-'))
		i=1;
	if(strcspn(src+1,char_tok)+i==strlen(src)){
		return 2;
	}
	// Checks done copy src
	strcpy(msg,src);
	
	// Get the position of tok
	while (msg[i++] != '\0'){
		if (msg[i] == tok)
			break;
	}
	
	if (msg[i-1] == '\0'){
		return 1;
	}
	// safe token position
	token_position=i;
		
	i++;	
	// if token is * the second inf could be -
	if((tok == '*' || tok == '/') && msg[i]=='-')
		i++;
	//then read the rest of the number
	while (msg[i++] != '\0'){
		if (!strspn(msg+i,num_tok))
			break;
	}
		
	i--;
	
	count_stop=i;
	//copy var b
	strncpy(b,msg+token_position+1,count_stop-token_position);
	b[count_stop-token_position]=0;
	// get var a
	i=token_position;
	
	i--;
	
	while (strspn(msg+i,"1234567890.") && i>=0)
		i--;
	
	
	
	if(msg[i-1] == '-')
			i--;
	if (i<0)
		i=0;
	
	count_start = i;
	strncpy(a,msg+count_start,token_position-count_start);
	a[token_position]=0;
	fa=atof(a);
	fb=atof(b);
	
	val=Calc_AB(tok,fa,fb);

	
	
	if(count_start>0){
		strncpy(part1,msg,count_start);
		part1[count_start]='\0';
	}else{
		part1[0]='\0';
	}
	
	if(count_stop<strlen(msg)-1){
		strcpy(part2,msg+count_stop+1);
		part2[strlen(msg)-count_stop]='\0';
	}else{
		part2[0]='\0';
	}
	
#ifdef DEBUG
	printf("source: %s | val: %f\n",src,val);
#endif	

	if (strlen(part1)==0 && strlen(part2)==0){
		sprintf(src,"%f",val);
		return 2;
	}else if (strlen(part1) && strlen(part2)==0 && val<0){
		sprintf(src,"%s%f",part1,val);
		return 0;
	}else if (strlen(part1) && strlen(part2)==0 && val>=0){
		sprintf(src,"%s+%f",part1,val);
		return 0;
	}else if (strlen(part1) && strlen(part2) && val<0){
		sprintf(src,"%s%f%s",part1,val,part2);
		return 0;
	}else if (strlen(part1) && strlen(part2) && val>=0){
		sprintf(src,"%s+%f%s",part1,val,part2);
		return 0;
	}else if (strlen(part1)==0 && strlen(part2) ){
		sprintf(src,"%f%s",val,part2);
		return 0;
	}
	return -1;
}


/*
	returns:
	 1: for no error
	-1: error in Check_For_Double_Tokens
	-2: error in Calc_String
	-3: we reached the end i guess
	
*/
int Solve_String (char *src){
	float status =0 ;
	
	while(!status){
		status = Check_For_Double_Tokens(src);
		if (status==-1)
			return -1;
		#ifdef DEBUG
		printf("* LOOP\n");
		#endif
		status = Calc_String(src,'*');
		#ifdef DEBUG
		printf("%s\n",src);
		#endif
		if (status==-1)
			return -2;
		if (status==2)
			return 1;
	}
	status =0 ;
	while(!status){
		status = Check_For_Double_Tokens(src);
		if (status==-1)
			return -1;
		#ifdef DEBUG
		printf("/ LOOP\n");
		#endif
		status = Calc_String(src,'/');
		#ifdef DEBUG
		printf("%s\n",src);
		#endif
		if (status==-1)
			return -2;
		if (status==2)
			return 1;
	}
	status =0 ;
	while(!status){
		status = Check_For_Double_Tokens(src);
		if (status==-1)
			return -1;
		#ifdef DEBUG
		printf("- LOOP\n");
		#endif
		status = Calc_String(src,'-');
		#ifdef DEBUG
		printf("%s\n",src);
		#endif
		if (status==-1)
			return -2;
		if (status==2)
			return 1;
	}
		status =0 ;
	while(!status){
		status = Check_For_Double_Tokens(src);
		if (status==-1)
			return -1;
		#ifdef DEBUG
		printf("+ LOOP\n");
		#endif
		status = Calc_String(src,'+');
		#ifdef DEBUG
		printf("%s\n",src);
		#endif
		if (status==-1)
			return -2;
		if (status==2)
			return 1;
	}
		status =0 ;
	#ifdef DEBUG
		printf("reached the end without doing smth :D %s\n",src);
	#endif
	return 1;	
}


/*
returns:
	 0: if no error occured
	-1:	error while removing spaces 
	-2: error while checking bracket count
	-3: error while solcing the bracket
	-4:	error during Solve_String (error in Check_For_Double_Tokens)
	-5:	error during Solve_String (error in Calc_String)
	-6: unrecognized chars in the src string
	-7: 
*/
int eval_string_float (char *src){
	int status,status2;
	status=Remove_Spaces(src);
	if (status==-1)
		return -1;
	
	status=Check_Tokens(src);
		if (status==-1)
		return -6;
	
	while((status=Check_Brackets(src))){
		if(status>0){
				status2=Solve_Brackets(src);
				if (status2 == -1)
					return -3;
		}else if(status==-1){
			return -2;
			
		}
	}
	status=Solve_String(src);
	if (status == -1 )
		return -4;
	if (status == -2 )
		return -5;
	
	return 0;

}

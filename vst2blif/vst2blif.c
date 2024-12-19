static char rcsid[] = "$Id: vst2blif.c,v 0.28 1994/05/09 10:17:42 archiadm Exp $";
static char build_date[] = "9 May 94";
/*
 * $Header: /home/cad00/users/archiadm/.Master/Filter/vst2blif.c,v 0.28 1994/05/09 10:17:42 archiadm Exp $
 * $Revision: 0.28 $
 * $Source: /home/cad00/users/archiadm/.Master/Filter/vst2blif.c,v $
 * $Log: vst2blif.c,v $
 * Revision 0.28  1994/05/09  10:17:42  archiadm
 * alcuni ritocchi qua' e la'
 *
 * Revision 0.27  1994/05/06  15:45:16  Rob
 * rimesso a posto l'help iniziale con l'aggiunta dell'id
 *
 * Revision 0.26  1994/04/20  17:26:02  blaurea1
 * ...non ricordo cosa ho cambiato, forse l'exit alla fine... non ricordo
 *
 * Revision 0.25  1994/04/19  10:19:12  blaurea1
 * aggiunta inizializzazione dei latch
 *
 * Revision 0.24  1994/04/19  08:14:40  blaurea1
 * oooops mancava l'exit dopo l'help
 *
 * Revision 0.23  1994/04/14  21:17:40  Rob
 * aggiunti i $Log: vst2blif.c,v $
 * Revision 0.28  1994/05/09  10:17:42  archiadm
 * alcuni ritocchi qua' e la'
 *
 * Revision 0.27  1994/05/06  15:45:16  Rob
 * rimesso a posto l'help iniziale con l'aggiunta dell'id
 *
 * Revision 0.26  1994/04/20  17:26:02  blaurea1
 * ...non ricordo cosa ho cambiato, forse l'exit alla fine... non ricordo
 *
 * Revision 0.25  1994/04/19  10:19:12  blaurea1
 * aggiunta inizializzazione dei latch
 *
 * Revision 0.24  1994/04/19  08:14:40  blaurea1
 * oooops mancava l'exit dopo l'help
 *.. => vedi precedente revisione
 *
 *
 *    Vst2Blif   version 0.0
 *    BY rAMBALDI rOBERTO.
 *       Feb. 25 1994.
 */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <unistd.h>


#define MAXTOKENLEN 256
#define MAXNAMELEN 32

enum TOKEN_STATES { tZERO, tTOKEN, tREM1, tREM2, tSTRING, tEOF };


/*                                                                     *
 * These are the test used in the tokenizer, please refer to the graph *
 * included in the documentation doc.                                  *
 *                                                                     */

/*           ASCII seq:   (  )  *  +  ,    :  ;  <  =  >       *
 *           code         40 41 42 43 44   58 59 60 61 62      */
#define isSTK(c) (( ((c)=='[') || ((c)==']') || ((c)=='|') || ((c)=='#') || \
		    ( ((c)>=':') && ((c)<='>') ) || \
		    ( ((c)>='(') && ((c)<=',') ) ))
#define isBLK(c) ( (((c)=='\t') || ((c)==' ') || ((c)=='\r')) )
#define isREM(c) ( ((c)=='-') )
#define isDQ(c)  ( ((c)=='"') )
#define isEOL(c) ( ((c)=='\n') )
#define isEOF(c) ( ((c)=='\0') )
#define isEOT(c) ( (isBLK((c)) || isEOL((c))) )


/* structure that contains all the info extracted from the library *
 * All the pins, input, output and clock (if needed)               *
 * This because SIS should be case sensitive and VHDL is case      *
 * insensitive, so we must keep the library's names of gates and   *
 * pins. In the future this may be useful for further checks that  *
 * in this version are not performed.                              */

/* list of inputs */
struct PortName{
    char             name[MAXNAMELEN];
    int             notused;  /* this flag is used to check if a  *
				* formal terminal has been already *
				* connected.                       */
    struct PortName  *next;
};

/* main structure */    
struct Cell {
    char            name[MAXNAMELEN];
    struct PortName *output;
    struct PortName *inputs;
    struct PortName *clock;
    struct Cell     *next;
};



/* list of formal-names, is used in GetPort when multiple   *
 * definition are gives, as    a,b,c : IN BIT;              */
struct formals {
    char   name[MAXNAMELEN];
    int   last;
    struct formals *next;
};

/* list of the conections in a single instance, the first   *
 * element should be the output port                        */
struct connections {
    char   formal[MAXNAMELEN];
    char   actual[MAXNAMELEN];
    struct connections *next;
};


FILE *In;
FILE *Out;
int  line;            /* line parsed                         */

int SendTokenBack;   /* Used to send the token back to the  *
                       * input stream in case of a missing   *
                       * keyword,                            */
char VDD[MAXNAMELEN]={ 0 },VSS[MAXNAMELEN]={ 0 },CLOCK[MAXNAMELEN]={ 0 };
int DEBUG;
int LOWERCASE;
int INIT;
int LINKAGE;
struct Cell *cells;


/* -=[ CloseAll ]=-                                 *
 * Closes all the files and flushes the used memory *
 *		                                    */
void CloseAll()
{

    if (In!=stdin) (void) fclose(In);
    if (Out!=stdout) (void) fclose(Out);

    free(cells);

};


/* -=[ Error ]=-                                    *
 * Displays an error message, and then exit         *
 *                                                  *
 * Input :                                          *
 *     msg  = message to printout before exiting    */
void Error(Msg)
char *Msg;
{
    (void) fprintf(stderr,"*** Error : %s\n",Msg);
    CloseAll();
    exit(1);
};

/* -=[ KwrdCmp ]=-                                  *
 * Compares to strings, without taking care of the  *
 * case.                                            *
 *                                                  *
 * Input :                                          *
 *      name = first string, typically the token    *
 *      keywrd = second string, typically a keyword *
 * Output :                                         *
 *      char = 1 if the string matches            *
 *              0 if they don't match               */
int KwrdCmp(name,keywrd)
char *name;
char *keywrd;
{
    int  t;
    int  len;

    if ( (len=strlen(name)) != (strlen(keywrd))) return 0;
    if (!len) return 0;
       /* if length 0 exit */
    for(t=0; (t<len); name++, keywrd++, t++){
	   /* if they're not equal */
	if (toupper(*name)!=toupper(*keywrd))
	    return 0;
	/* EoL, exit */
	if (*name=='\0') break;
    };
    return 1;
}


/* -=[ GetLibToken ]=-                              *
 * Tokenizer to scan the library file               *
 *                                                  *
 * Input  :                                         *
 *      Lib = library file                          *
 * Output :                                         *
 *      tok = filled with the new token             */
void GetLibToken(Lib,tok)
FILE *Lib;
char *tok;
{
    enum states { tZERO, tLONG, tEOF, tSTRING };
    static enum states next;
    static int init=0;
    static int sentback;
    static char TOKEN[MAXTOKENLEN];
    static int str;
    int   ready;
    int    num;
    char   *t;
    int   c;

    if (!init){
	sentback=0;
	init=1;
	next=tZERO;
	str=0;
    };

    t= &(TOKEN[0]);
    num=0;
    str=0;

    do{
	if (sentback){
	    c=sentback;
	} else {
	    c=fgetc(Lib);
	};
	if (feof(Lib)) next=tEOF;
	ready=0;
	sentback='\0';

	switch (next) {
	  case tZERO:
	    if ((c==' ') || (c=='\r') || (c=='\t')){
		next=tZERO;
	    } else {
		if ( ((c>=0x27) && (c<=0x2b)) || (c=='=') || (c==';') || (c=='\n') || (c=='!')){
		    *t=c; t++;
		    next=tZERO;
		    ready=1;
		} else {
		    if (c=='"') {
			num=0;
			next=tSTRING;
		    } else
			{
			    num=0;
			    next=tLONG;
			    ready=0;
			    sentback=c;
			};
		};
	    };
	    break;
	  case tLONG:
	    if ((c==' ') || (c=='\r') || (c=='\t')){
		ready=1;
		next=tZERO;
	    } else {
		if ( ((c>=0x27) && (c<=0x2b)) || (c=='=') || (c==';') || (c=='\n') || (c=='!')){
		    next=tZERO;
		    ready=1;
		    sentback=c;
		} else {
		    if (c=='"') {
			ready=1;
			next=tSTRING;
		    } else {
			*t=c;
			t++; num++;
			next=tLONG;
			if ( (ready=(num>=MAXTOKENLEN-1)) )
			    (void) fprintf(stderr,"Sorry, exeeded max name len of %u",num+1);
		    };
		};
		    
	    };
	    break;
	  case tSTRING:
	    if (!str) {
		*t='"'; t++; num++;
		str=1;
	    };
	    *t=c; t++; num++;
	    if (c=='"') {   /* last dblquote */
		ready=1;
		next=tZERO;
		break;
	    };
	    next=tSTRING;
	    if ( (ready=(num>=MAXTOKENLEN-1)) )
		(void) fprintf(stderr,"Sorry, exeeded max name len of %u",num+1);
	    break;
	  case tEOF:
	    next=tEOF;
	    ready=1;
	    sentback=c;
	    *t=c;
	  default :
	      next=tZERO;
	};
    } while(!ready);
    *t='\0';
    (void) strcpy(tok,&(TOKEN[0]));
};

/* -=[ IsValidInput ]=-                             *
 * Check is a name has been already used in the     *
 * expression. Here we look only at names, 'unused' * 
 * flag here is not useful.                         *
 *                                                  *
 * Input  :                                         *
 *     pins = pointer to list of formals            *
 *     name = name to check                         *
 * Output :                                         *
 *     1 = if 'name' is not used                    *
 *     0 = if used                                  */
int IsValidInput(pins,name)
struct PortName *pins;
char *name;
{
    
    while(pins!=NULL){
	/* We *MUST* use KwrdCmp because VHDL is *NOT* case sensitive */
	if (KwrdCmp(pins->name,name)){
	    return 0;
	};
	pins=pins->next;
    };
    return 1;
}


/* -=[ ScanLibrary ]=-                              *
 * Scans the library to get the names of the cells  *
 * the output pins and the clock signals of latches *
 *                                                  *
 * Input :                                          *
 *     LibName = the name of library file           */
void ScanLibrary(LibName)
char *LibName;
{
    FILE *Lib;
    struct Cell *pC;
    struct PortName *pPN;
    char LocalToken[MAXTOKENLEN];
    char tmp[MAXNAMELEN];
    int firstPN;
    int firstC;
    int latch;
    int IsCell;
    char *s;
    

    if ((Lib=fopen(LibName,"rt"))==NULL)
	Error("Couldn't open library file");


    s= &(LocalToken[0]);
    firstC=1;
    latch=0; IsCell=0;
    (void) fseek(Lib,0,SEEK_SET);
    if ( (cells=(struct Cell *)calloc(1, sizeof(struct Cell)) ) == NULL) 
	Error("Allocation error not enought memory !");
    do {
	GetLibToken(Lib,s);
	if (KwrdCmp(s,"GATE")) {
	    if (latch) {
                (void) fprintf(stderr,"No CONTROL keyword for %s latch\n",pC->name);
                Error("Could not continue");
            };
	    latch=0;
	    IsCell=1;
	} else {
	    if (KwrdCmp(s,"LATCH")) {
		if (latch) {
		    (void) fprintf(stderr,"No CONTROL keyword for %s latch\n",pC->name);
		    Error("Could not continue");
		};
		IsCell=1;
		latch=1;
	    } else {
                if (KwrdCmp(s,"CONTROL")) {
                    latch=0;
		    if ( (pC->clock=(struct PortName *)calloc(1, sizeof(struct PortName)) ) == NULL) 
			Error("Allocation error not enought memory !");
		    (pC->clock)->next=NULL;
		    /* get name of control signal */
		    GetLibToken(Lib,s);
                    (void) strncpy((pC->clock)->name,s,MAXNAMELEN);
                };
		
	    };
	};
	if (IsCell) {
	    if (firstC) {
		pC=cells;
		firstC=0;
	    } else {
		if ( (pC->next=(struct Cell *)calloc(1, sizeof(struct Cell)) ) == NULL) 
		    Error("Allocation error not enought memory !");
		pC=pC->next;
	    };

	    /* name */
	    GetLibToken(Lib,s);
	    (void) strncpy(pC->name,s,MAXNAMELEN);

	    /* skip Area */
	    GetLibToken(Lib,s);

	    /* output */
	    GetLibToken(Lib,s);
	    if ( (pC->output=(struct PortName *)calloc(1, sizeof(struct PortName)) ) == NULL) 
		Error("Allocation error not enought memory !");
	    (pC->output)->next=NULL;
	    (pC->output)->notused=1;
	    (void) strncpy( (pC->output)->name ,s, MAXNAMELEN);

	    /* get inputs */
	    firstPN=1;
	    do {
		GetLibToken(Lib,s);
		if ( !( ((*s>=0x27) && (*s<=0x2b)) || (*s=='=') || (*s=='!')|| (*s==';')) ){
		    (void) strncpy(tmp,s,5);
		    tmp[5] = '\0';
		    if (KwrdCmp(tmp,"CONST") && !isalpha(*(s+6)))
			/* if the expression has a constant value we must */
			/* skip it, because there are no inputs           */
			break;
		    /* it's an operand so get its name */
		    if (IsValidInput(pC->inputs,s)) {
			/* only if it was not used in this expression */
			if (firstPN){
				if ( (pC->inputs=(struct PortName *)calloc(1, sizeof(struct PortName)) ) == NULL) 
				Error("Allocation error not enought memory !");
			    pPN=pC->inputs;
			    firstPN=0;
			} else {
			    if ( (pPN->next=(struct PortName *)calloc(1, sizeof(struct PortName)) ) == NULL) { 
				Error("Allocation error not enought memory !");
			    };
			    pPN=pPN->next;
			    pPN->next=NULL;
			};
			(void) strncpy(pPN->name,s,MAXNAMELEN);
			pPN->notused=1;
		    };
		};
	    } while ( (*s!=';') );
	    IsCell=0;
	};
    } while (!feof(Lib));
};



/* -=[ CheckArgs ]=-                                *
 * Gets the options from the command line, open     *
 * the input and output file and read params from   *
 * the library file.                                *
 *                                                  *
 * Input :                                          * 
 *     argc,argv = usual cmdline arcguments         */
void CheckArgs(argc,argv)
int  argc;
char **argv;
{
    int c;
    int help;
    char *s;
    int NoPower;

    extern char *optarg;
    extern int optind;

    (void) fprintf(stderr,"\t\t         Vst2Blif v1.0\n");
    (void) fprintf(stderr,"\t\t      by Roberto Rambaldi\n");
    (void) fprintf(stderr,"\t\tD.E.I.S. Universita' di Bologna\n\n");
    help=0; NoPower=0; LINKAGE='i';
    LOWERCASE=1; INIT='3';
    while( (c=getopt(argc,argv,"s:S:d:D:c:C:i:I:l:L:hHvVuUnN$") )>0 ){
	switch (toupper(c)) {
	  case 'S':
	    (void) strncpy(VSS,optarg,MAXNAMELEN);
	    break;
	  case 'D':
	    (void) strncpy(VDD,optarg,MAXNAMELEN);
	      break;
	  case 'V':
	    DEBUG=1;
	    break;
	  case 'H':
	    help=1;
	    break;
	  case 'C':
	    (void) strncpy(CLOCK,optarg,MAXNAMELEN);
	    break;
	  case 'I':
	    INIT= *optarg;
	    if ((INIT<'0') || (INIT>'3')) {
		(void) fprintf(stderr,"Wrong latch init value");
		help=1;
	    };
	    break;
	  case 'L':
	    if (KwrdCmp(optarg,"IN")) {
		LINKAGE='i';
	    } else {
		if  (KwrdCmp(optarg,"OUT")) {
		    LINKAGE='o';
		} else {
		    (void) fprintf(stderr,"\tUnknow direction for a port of type linkage\n");
		    help=1;
		};
	    };
	    break;
	  case 'U':
	    LOWERCASE=0;
	    break;
	  case 'N':
	    NoPower=1;
	    break;
	  case '$':
	    help=1;
	    (void) fprintf(stderr,"\n\tID = %s\n",rcsid);
	    (void) fprintf(stderr,"\tCompiled on %s\n\n",build_date);
	    break;
	};
    };	    


    if (!help) {
	if (LOWERCASE) 
	    for(s= &(CLOCK[0]); *s!='\0'; s++) tolower(*s);
	else 
	    for(s= &(CLOCK[0]); *s!='\0'; s++) toupper(*s);
	
	if (optind>=argc) {
	    (void) fprintf(stderr,"No Library file specified\n\n");
	    help=1;
	} else {
	    ScanLibrary(argv[optind]);
	    if (++optind>=argc){
		In=stdin; Out=stdout;
	    } else {
		if ((In=fopen(argv[optind],"rt"))==NULL) {
		    (void) fprintf(stderr,"Couldn't read input file");
		    help=1;
		};
		if (++optind>=argc) { Out=stdout; }
		else {
		    if ((Out=fopen(argv[optind],"wt"))==NULL) {
			(void) fprintf(stderr,"Could'n make opuput file");
			help=1;
		    };
		};
	    };
	};
	
	if (NoPower) {
	    VDD[0]='\0'; VSS[0]='\0';
	} else {
	    if (VDD[0]=='\0') (void) strcpy(VDD,"VDD");
	    if (VSS[0]=='\0') (void) strcpy(VSS,"VSS");
	};
    };
	
    if (help) {
	(void) fprintf(stderr,"\tUsage: vst2blif [options] <library> [infile [outfile]]\n");
	(void) fprintf(stderr,"\t\t if outfile is not given stdout will be used, if infile\n");
	(void) fprintf(stderr,"\t\t is also not given stdin will be used instead.\n");
	(void) fprintf(stderr,"\t<library>\t is the name of the library file to use\n");
	(void) fprintf(stderr,"\tOptions :\n\t-s <name>\t <name> will be used for VSS net\n");
	(void) fprintf(stderr,"\t-d <name>\t <name> will be used for VDD net\n");
	(void) fprintf(stderr,"\t-c <name>\t .clock <name>  will be added to the blif file\n");
	(void) fprintf(stderr,"\t-i <value>\t default value for latches, must be between 0 and 3\n");
	(void) fprintf(stderr,"\t-l <in/out>\t sets the direction for linkage ports\n");
	(void) fprintf(stderr,"\t\t\t the default value is \"in\"\n");
        (void) fprintf(stderr,"\t-u\t\t converts all names to uppercase\n");
	(void) fprintf(stderr,"\t-n\t\t no VSS or VDD to skip.\n");
	(void) fprintf(stderr,"\t-h\t\t prints these lines");
	(void) fprintf(stderr,"\n\tIf no VDD or VSS nets are given VDD and VSS will be used\n");
	exit(0);
    };
};


/* -=[ GetNextToken ]=-                             *
 * Tokenizer, see the graph to understand how it    *
 * works.                                           *
 *                                                  *
 * Inputs :                                         *
 *     tok = pointer to the final buffer, which is  *
 *           a copy of the internal Token, it's     *
 * *indirectly* uses SendTokenBack as input and     *
 * line as output                                   */
void GetNextToken(tok)
char *tok;
{
    static int init=0;
    static enum TOKEN_STATES state;
    static int sentback;
    static int str;
    static char Token[MAXTOKENLEN];
    char   *t;
    int    num;
    int   TokenReady;
    int   c;
    
    if (!init) {
	state=tZERO;
	init=1;
	line=0;
	SendTokenBack=0;
    };

    t= &(Token[0]);
    num=0;
    TokenReady=0;
    str=0;

    if (SendTokenBack) {
	SendTokenBack=0;
	(void) strcpy(tok,Token);
	return;
    };

    do {
	if (sentback) {
	    c=sentback;
	} else {
	    c=fgetc(In);
	    if (feof(In)) state=tEOF;
	    if (c=='\n') line++;
	};

	switch (state){
	  case tZERO:
            /*******************/
	    /*    ZERO state   */
            /*******************/
	    num=0;
	    sentback='\0';
	    if (isSTK(c)) {
		*t=c; t++;
		TokenReady=1;
	    } else {
		if isREM(c) {
		    *t=c;
		    t++; num++;
		    sentback='\0';
		    state=tREM1;
		} else {
		    if isDQ(c) {
			sentback='\0';
			state=tSTRING;
		    } else {
			if isEOF(c) {
			    state=tEOF;
			    sentback=1;
			    TokenReady=0;
			} else {
			    if isEOT(c) {
				state=tZERO;
				/* stay in tZERO */
			    } else {
				sentback=c;
				state=tTOKEN;
			    };
			};
		    };
		};
	    };
	    break;
	  case tTOKEN:
            /*******************/
	    /*   TOKEN  state  */
            /*******************/
	    TokenReady=1;
	    sentback=c;
	    if (isSTK(c)) {
		state=tZERO;
	    } else {
		if isREM(c) {
		    state=tREM1;
		} else {
		    if isDQ(c) {
			sentback='\0';
			state=tSTRING;
		    } else {
			if isEOF(c) {
			    sentback=1;
			    state=tEOF;
			} else {
			    if isEOT(c) {
				sentback='\0';
				state=tZERO;
			    } else {
				sentback='\0';
				if (num>=(MAXTOKENLEN-1)){
				    sentback=c;
				    (void) fprintf(stderr,"*Parse Warning* Line %u: token too long !\n",line);
				} else {
				    if (LOWERCASE)
					*t=tolower(c);
				    else 
					*t=toupper(c);
				    t++; num++;
				    TokenReady=0;
				    /* fprintf(stderr,"."); */
				};
				state=tTOKEN;
			    };
			};
		    };
		};
	    };
	    break;
	  case tREM1:
            /*******************/
	    /*    REM1 state   */
            /*******************/
	    TokenReady=1;
	    sentback=c;
	    if (isSTK(c)) {
		state=tZERO;
	    } else {
		if isREM(c) {
		    sentback='\0';
		    state=tREM2;    /* it's a remmark. */
		    TokenReady=0;
		} else {
		    if isDQ(c) {
			sentback='\0';
			state=tSTRING;
		    } else {
			if isEOF(c) {
			    state=tEOF;
			    sentback=1;
			    TokenReady=0;
			} else {
			    if isEOT(c) {
				sentback='\0';
				/* there's no need to parse an EOT */
				state=tZERO;
			    } else {
				state=tTOKEN;
			    };
			};
		    };
		};
	    };
	    break;
	  case tREM2:
            /*******************/
	    /*    REM2 state   */
            /*******************/
	    sentback='\0';
	    TokenReady=0;
	    if isEOL(c) {
		num=0;
		t= &(Token[0]);
		state=tZERO;
	    } else {
		if isEOF(c) {
		    state=tEOF;
		    sentback=1;
		    TokenReady=0;
		} else {
		    state=tREM2;
		};
	    };
	    break;
	  case tSTRING:
            /*******************/
	    /*  STRING  state  */
            /*******************/
	    if (!str) {
		*t='"'; t++; num++; /* first '"' */
		str=1;
	    };
	    sentback='\0';
	    TokenReady=1;
	    if isDQ(c) {
		*t=c; t++; num++;
		state=tZERO;   /* There's no sentback char so this     *
				* double quote is the last one *ONLY*  */
	    } else {
		if isEOF(c) {
		    state=tEOF;    /* this is *UNESPECTED* ! */
		    sentback=1;
		    (void) fprintf(stderr,"*Parse Warning* Line %u: unespected Eof\n",line);
		} else {
		    sentback='\0';
		    if (num>=MAXTOKENLEN-2){
			sentback=c;
			(void) fprintf(stderr,"*Parse Warning* Line %u: token too long !\n",line);
		    } else {
			*t=c; t++; num++;
			TokenReady=0;
			state=tSTRING;
		    };
		};
	    };
	    break;
	  case tEOF:
            /*******************/
	    /*    EOF  state   */
            /*******************/
	    t= &(Token[0]);
	    TokenReady=1;
	    state=tEOF;
	    break;
	};
    } while(!TokenReady);
    *(t)='\0';
    (void) strcpy(tok,Token);
    return ;
};


/* -=[ WhatGate ]=-                                 *
 * Returns a pointer to an element of the list      *
 * of gates that matches up the name given, if      *
 * there isn't a match a null pointer is returned   *
 *                                                  *
 * Input :                                          *
 *     name = name to match                         *
 * Ouput :                                          *
 *     (void *) a pointer                           */
void *WhatGate(name)
char *name;
{
    struct Cell  *ptr;

    for(ptr=cells; ptr!=NULL ; ptr=ptr->next)
	if ( KwrdCmp(ptr->name,name) ) return (void *)ptr;

    return (void *)NULL;
};

/* -=[ Warning ]=-                                  *
 * Puts a message on stderr, write the current line *
 * and then sends the current token back            *
 *                                                  *
 * Inputs :                                         *
 *      name = message                              */
void Warning(name)
char *name;
{
    if (DEBUG) (void) fprintf(stderr,"*parse warning* Line %u : %s\n",line,name);
    SendTokenBack=1;
};

/* -=[ VstError ]=-                                 *
 * sends to stderr a message and then gets tokens   *
 * until a given one is reached                     *
 *                                                  *
 * Input :                                          *
 *     name = message to print                      *
 *     next = token to reach                        */ 
void VstError(name,next)
char *name;
char *next;
{
    char *w;
    char LocalToken[MAXTOKENLEN];

    w= &(LocalToken[0]);
    (void) fprintf(stderr,"*Error* Line %u : %s\n",line,name);
    (void) fprintf(stderr,"*Error* Line %u : skipping text until the keyword %s is reached\n",line,next);
    SendTokenBack=1;
    do{
	GetNextToken(w);
	if (feof(In))
	    Error("Unespected Eof!");
    } while( !KwrdCmp(w,next) );
};

/* -=[ DecNumber ]=-                                *
 * checks if a token is a decimal number            *
 *                                                  *
 * Input  :                                         *
 *     string = token to check                      *
 * Output :                                         *
 *     int = converted integer, or 0 if the string  *
 *           is not a number                        *
 * REMMARK : strtol() can be used...                */
int DecNumber(string)
char *string;
{
    char msg[50];
    char *s;

    for(s=string; *s!='\0'; s++)
	if (!isdigit(*s)) {
	    (void) sprintf(msg,"*Error Line %u : Expected decimal integer number \n",line);
	    Error(msg);
	};
    return atoi(string);
};

/* -=[ GetVector ]=-                                *
 * Gets the extremes of a vector definition         *
 *                                                  *
 * Ouput :                                          *
 *     frst : first element                         *
 *     scnd : second element                        *
 * :: BIT_VECTOR(7 DOWNTO 0) --> frst=7, scnd=0     *
 *              (0 TO 7)     --> frst=0, scnd=7     */
void GetVector(frst,scnd)
int *frst;
int *scnd;
{
    char *w;
    char LocalToken[MAXTOKENLEN];
    int vect_to;
    int vect_down;


    w= &(LocalToken[0]);
    vect_to=0; vect_down=0;

    /* start: */
    GetNextToken(w);
    if (*w!='(') Warning("expected '(' for vector size");

    /* first number */
    GetNextToken(w);
    *frst=DecNumber(w);

    /* Upward or downward ? */
    GetNextToken(w);
    vect_to=KwrdCmp(w,"TO");
    vect_down=KwrdCmp(w,"DOWNTO");
    if (!vect_to && !vect_down)
	Warning("expected kewords TO or DOWNTO for vector size");

    /* second number */
    GetNextToken(w);
    *scnd=DecNumber(w);


    if (*frst==*scnd) Warning("vector with no elements !?");
    if (vect_to) {
	if (*frst>*scnd) Warning("used keyword TO instead of DOWNTO");
    } else
	if (vect_down) {
	  if (*frst<*scnd) Warning("used keyword DOWNTO instead of TO");
    };
    SendTokenBack=0; /* I know that this is really bad, SendTokenBack     *
		      * should be used only by Warning() GetNextToken()   *
		      * and VstError() but ....                           *
		      * this way is quikly & simple ... :-)               */

    /* closing ')' */
    GetNextToken(w);
    if (*w!=')') Warning("expected ')' for vector size");
}


/* -=[ PrintVector ]=-                              *
 * Get the extremes of a vector and then expandes   *
 * all of the elements.                             *
 *                                                  *
 * Input :                                          *
 *     first = formals which have the same          *
 *             definition                           *
 *     flag  = for printing                         */
void PrintVector(first,flag)
struct formals *first;
int flag;
{
    struct formals *ptr;
    int  j;
    int  frst;
    int  scnd;
    int tmp;

    /* first get vector size */
    GetVector(&frst,&scnd);

    if (flag){
	ptr=first;
	do {
	/* Now let's print em */
	    if (frst<scnd) {
		for(j=frst; j<=scnd; j++)
		    (void) fprintf(Out,"%s(%u) ",ptr->name,j);
	    } else {
		for(j=frst; j>=scnd; j--)
		    (void) fprintf(Out,"%s(%u) ",ptr->name,j);
	    };
	    tmp=ptr->last;
	    ptr=ptr->next;
	} while(tmp);
    };
}

/* -=[ FreeFormals ]=-                             *
 * Releases the memory used for parsing multiple   *
 * declarations.                                   *
 *                                                 *
 * Input :                                         *
 *     first = first element of the list, this     *
 *             should *NOT* be released because    *
 *             it is declared statically           */
void FreeFormals(first)
struct formals *first;
{
    struct formals *tmp;
    struct formals *ptr;

    ptr=first->next;
    if (ptr==NULL) return;
    for (; ptr!=NULL; ) {
	tmp=ptr->next;
	free(ptr);
	ptr=tmp;
    };
};

/* -=[ GetPort ]=-                                 *
 * Gets the port definition of an ENTITY or of a   *
 * COMPONENT.                                      *
 *                                                 *
 * Input :                                         *
 *     flag = 1 for the ENTITY                     *
 *            0 for the COMPONENT                  */
void GetPort(flag)
int flag;
{
    struct formals first;
    struct formals *ptr;
    char *w;
    char LocalToken[MAXTOKENLEN];
    int dir;
    int dir2;
    int alim;
    int tmp;


    w= &(LocalToken[0]);
    first.next=NULL;

    GetNextToken(w);
    if ( *w!='(' ) Warning("expected '('");

    dir = '\0'; alim='\0';
    do{
	/* name of the port */
	GetNextToken(w);
	(void) strcpy(first.name,w);
	first.last=1;
	alim= (KwrdCmp(w,VDD) || KwrdCmp(w,VSS) || KwrdCmp(w,CLOCK) );
	/* multiple definition */
	ptr= &first;
	do {
	    GetNextToken(w);
	    if (*w==':') {
		ptr->last=0;
		break;
	    };
	    if (*w!=',') {
		FreeFormals(&first);
		VstError("expected ':' or ',' after identifier",";");
	    };
	    /* next formal name */
	    GetNextToken(w);

	    if (flag && !(KwrdCmp(w,VDD) || KwrdCmp(w,VSS) || KwrdCmp(w,CLOCK))) {
		if (ptr->next==NULL) {
		    if ( (ptr->next=(struct formals *)calloc(1,sizeof(struct formals)))==NULL ){
			FreeFormals(&first);
			Error("Allocation error or not enought memory");
		    };
		    (ptr->next)->next=NULL;
		};
		ptr=ptr->next;
		(void) strcpy(ptr->name,w);
		ptr->last=1;
	    };
	} while (1);

	/* direction: in,out,inout */
	GetNextToken(w);
	if (KwrdCmp(w,"IN")){
	    dir2='i';
	} else {
	    if (KwrdCmp(w,"OUT")) {
		dir2='o';
	    } else {
		if (KwrdCmp(w,"LINKAGE")) {
		    dir2=LINKAGE;
		} else {
		    if (KwrdCmp(w,"INOUT")){
			(void) fprintf(stderr,"* Error * Line %u : input/output ports not supported\n",line);
			Error("Could not continue");
		    } else {
			(void) fprintf(stderr,"* Error * Line %u : unknown direction of a port\n",line);
			Error("Could not continue");
		    };
		};
	    };
	};

	/* If the direction changed print a new line */
	if (flag && !(alim && !first.last)) {
	    if (dir!=dir2){
		if (dir2=='i') (void) fprintf(Out,"\n.inputs ");
		else (void) fprintf(Out,"\n.outputs ");
	    };
	    dir=dir2;
	};

	/* type: bit, bit_vector */
	GetNextToken(w);
	if (KwrdCmp(w,"BIT")) {
	    if (flag && !(alim && !first.last)) {
		ptr= &first;
		do {
		    (void) fprintf(Out,"%s ",ptr->name);
		    tmp=ptr->last;
		    ptr=ptr->next;
		} while (tmp);
	    };
	} else {
	    /* Now let's look at vectors */
	    if (KwrdCmp(w,"BIT_VECTOR")) {
		if (!flag) {
		    Error("Sorry, but vectors in library gates are not allowed !\n");
		};
		/* vector of bits */
		PrintVector(&first,flag);
	    } else {
		(void) fprintf(stderr,"* Error * Line %u : Unknown type of signal : %s \n",line,w);
		Error("Could not continue");
	    };
	};
	GetNextToken(w);
	if (*w==')') break;
	if (*w!=';') {
	    VstError("expected ';' or ')'","END");
	    FreeFormals(&first);
	    return;
	};
    } while(1);
    GetNextToken(w);
    if (*w!=';') Warning("extected ';'");
    if (flag) {
	if (CLOCK[0])
	    (void) fprintf(Out,"\n.clock %s",CLOCK);
	(void) fprintf(Out,"\n\n");
	FreeFormals(&first);
    };
};


/* -=[ GetEntity ]=-                                *
 * parses the entity statement                      */
void GetEntity()
{
    char *w;
    char LocalToken[MAXTOKENLEN];
    char name[MAXTOKENLEN];
    int  num;

    w= &(LocalToken[0]);

    /* name of the entity = name of the model */
    GetNextToken(w);
    (void) strcpy(name,w);
    (void) fprintf(Out,".model %s\n",w);
    GetNextToken(w);
    if (!KwrdCmp(w,"IS")) Warning("expected syntax: ENTITY <name> IS");

    /* GENERIC CLAUSE */
    GetNextToken(w);
    if (KwrdCmp(w,"GENERIC")) {
	GetNextToken(w);
	if (*w!='(') Warning("expected '(' after GENERIC keyword");
	num=1;
	do {
	    GetNextToken(w);
	    if (*w=='(') num++;
	    else {
		if (*w==')') num--;
	    };
	} while (num!=0);
	GetNextToken(w);
	if (*w!=';') Warning("expected ';'");
	GetNextToken(w);
    };

    /* PORT CLAUSE */
    if (KwrdCmp(w,"PORT")) {
	GetPort(1);
    } else {
	Warning("no inputs or outputs in this entity ?!");
    };

    /* END CLAUSE */
    GetNextToken(w);
    if (!KwrdCmp(w,"END")) {
	VstError("Keyword missing !","END");
	/* something went wrong.. we will re-synchronize with  *
	 * the source text when we will reach the END keyword  */
    };

    GetNextToken(w);
    if (!KwrdCmp(w,name))
	Warning("<name> after END differs from <name> after ENTITY");

    GetNextToken(w);
    if (*w!=';') Warning("expected ';'");

};

/* -=[ GetComponent ]=-                             *
 * Parses the component statement                   */
void GetComponent()
{
    char *w;
    char LocalToken[MAXTOKENLEN];

    w= &(LocalToken[0]);
    /* component name */
    GetNextToken(w);

    /* A small checks may be done here ... next time */

    /* PORT CLAUSE */
    GetNextToken(w);
    if (KwrdCmp(w,"PORT")) {
	GetPort(0);
    } else {
	Warning("no inputs or outputs in this component ?!");
    };


    /* END CLAUSE */
    GetNextToken(w);
    if (!KwrdCmp(w,"END")) {
	Warning(" END keyword missing !");
    };

    GetNextToken(w);
    if (!KwrdCmp(w,"COMPONENT")) {
	Warning("COMPONENT keyword missing !");
    };

    GetNextToken(w);
    if (*w!=';') Warning("expected ';'");

};

/* -=[ GetSignal ]=-                                 *
 * Skips the signal definitions                      */
void GetSignal()
{
    char *w;
    char LocalToken[MAXTOKENLEN];

    /* We're not interested in signals definition, so we will skip *
     * all the entry --> just go to the next ';'                   */

    w= &(LocalToken[0]);
    do{
	GetNextToken(w);
	if (feof(In))
	    Error("Unespected Eof");
    } while (*w!=';');

}

/* -=[ GetName ]=-                                   *
 * Gets a name of an actual terminal, that can be    *
 * a single token or 3 tokens long (if it is an      *
 * element of a vector )                             *
 *                                                   *
 * Output :                                          *
 *     name = the name read                          */
void GetName(name)
char *name;
{
    char *w;
    char LocalToken[MAXTOKENLEN];

    w= &(LocalToken[0]);
    GetNextToken(w);
    (void) strcpy(name,w);
    GetNextToken(w);
    if (*w!='(') {
	SendTokenBack=1;
	/* Don't lose this token */
	return;
    };
    GetNextToken(w);
    (void) sprintf(name,"%s(%u)",name,DecNumber(w));
    GetNextToken(w);
    if (*w!=')') Warning("expected ')'");
};

/* -=[ FreeConns ]=-                               *
 * Releases the memory used for parsing the        *
 * connections .                                   *
 *                                                 *
 * Input :                                         *
 *     first = first element of the list, this     *
 *             should *NOT* be released because    *
 *             it is declared statically           */
void FreeConns(first)
struct connections *first;
{
    struct connections *tmp;
    struct connections *ptr;

    ptr=first->next;
    if (ptr==NULL) return;
    for (; ptr!=NULL; ) {
	tmp=ptr->next;
	free(ptr);
	ptr=tmp;
    };
};

/* -=[ ClearFormals ]=-                            *
 * sets the unused flag to '1', this is performed  *
 * each time the  cell is selected                 *
 *                                                 *
 * Input  :                                        *
 *     cell = pointer to the cell structure        */
void ClearFormals(cell)
struct Cell *cell;
{ 
    struct PortName *ptr;

    for(ptr=cell->inputs; ptr!=NULL; ptr=ptr->next){
	ptr->notused=1; 
    };
    
    (cell->output)->notused=1;
    if (cell->clock!=NULL) (cell->clock)->notused=1;

};

/* -=[ CheckFormals ]=-                            *
 * Checks if a terminal has been used and then     *
 * replaces its name with the une used in the cell *
 * definition.                                     *
 *                                                 *
 * Input  :                                        *
 *     cell = the cell (...)                       *
 *     name = name of formal to check              */
int CheckFormal(cell,name)
struct Cell *cell;
char   *name;
{
    struct PortName *ptr;
    
    for(ptr=cell->inputs; ptr!=NULL; ptr=ptr->next){
	if (ptr->notused) {
	    if (KwrdCmp(ptr->name,name)){
		ptr->notused=0;
	        (void) strcpy(name,ptr->name);
		return 1;
	    };
	};
    };
    
    ptr=cell->output;
    if (ptr->notused) {
	if (KwrdCmp(ptr->name,name)){
	    ptr->notused=0;
	    (void) strcpy(name,ptr->name);
	    return 1;
	};
    };
    
    ptr=cell->clock;
    if ((ptr!=NULL) && (ptr->notused)) {
	if (KwrdCmp(ptr->name,name)){
	    ptr->notused=0;
	    (void) strcpy(name,ptr->name);
	    return 1;
	};
    };

    return 0;
};


/* -=[ AllUsed]=-                                   *
 * Checks if all the formals are connected          *
 *                                                  *
 * Input  :                                         *
 *     cell = the cell to check;                    */
int AllUsed(cell)
struct Cell *cell;
{
    struct PortName *ptr;
    
    for(ptr=cell->inputs; ptr!=NULL; ptr=ptr->next){
	if (ptr->notused) return 0;
    };
    
    if ((cell->output)->notused) return 0;
     
    if ((cell->clock!=NULL) && ((cell->clock)->notused)) return 0;

    return 1;
};


/* -=[ GetInstance ]=-                              *
 * parses the netlist                               */
void GetInstance()
{
    struct connections first;
    struct connections *ptr;
    struct Cell        *ThisCell;
    char   *w;
    char   LocalToken[MAXTOKENLEN];
    int   alim;
    int   gate;
    int   clk;
    int   output;
    char   tmp[MAXTOKENLEN];
    char   clock[MAXTOKENLEN];
    int   err;

    
    w= &(LocalToken[0]);
    err=0;
    output=0;
    GetNextToken(w);
    if (*w!=':') Warning("Missing ':'");

    /* name of the gate to use now */
    GetNextToken(w);
    gate=1;
    if ((ThisCell=WhatGate(w))==NULL) {
	(void) sprintf(tmp,"Line %u : cell %s is *NOT* in the library",line,w);
	Error(tmp);
	/* This is the end ...                      *
	 * Is a gate or a latch ? we couldn't know  *
	 * so it's better to give up and wait for   *
	 * better times ..                          */
    };
    gate = (ThisCell->clock)==NULL;
    ClearFormals(ThisCell);

    /* We must use the names as they were written on the library file */
    if (gate) {
	(void) fprintf(Out,".gate %s ",ThisCell->name);
    } else {
	(void) fprintf(Out,".mlatch %s ",ThisCell->name);
    };

    GetNextToken(w);
    if (!KwrdCmp(w,"PORT")) {
	VstError("PORT keyword missing","PORT");
	err=1;
    };

    GetNextToken(w);
    if (!KwrdCmp(w,"MAP"))
	Warning("MAP keyword missing");

    GetNextToken(w);
    if (*w!='(')
	Warning("Expexcted '('");

    alim=0;
    first.next=NULL;
    first.formal[0]='\0';
    ptr= &first; clk=0;
    clock[0]='\0';
    do{
	if (!alim) {
	    /* if the previous one was a power line, just overwrite *
	     * the last pointer..                                   */
	    if ( (ptr->next=(struct connections *)calloc(1,sizeof(struct connections)))==NULL ){
		FreeConns(&first);
		Error("Allocation error or not enought memory");
	    };
	    ptr=ptr->next;
	    ptr->next=NULL;
	};

	/* Get the formal terminal */
	GetNextToken(w);
	alim= (KwrdCmp(w,VDD) || KwrdCmp(w,VSS)) ;
	if (!alim && !CheckFormal(ThisCell,w)) {
	    (void) sprintf(tmp,"formal terminal %s is not present into %s",w,ThisCell->name);
	    Error(tmp);
	};

	if (!strcmp((ThisCell->output)->name,w)) {
	    if ((first.formal)[0]!='\0') {
		Warning("Multiple output lines in this gate");
		SendTokenBack=0;
	    };
	    (void) strcpy(first.formal,w);
	    output=1;
	    alim=1;
	} else {
	    if ( (!gate) && (!strcmp(w,(ThisCell->clock)->name)) ) {
		clk=1;
		alim=1;
	    } else
		(void) strcpy(ptr->formal,w);
	};

	GetNextToken(w);
	SendTokenBack=(*w!='='); /* this is warning but we need to wait */

	GetNextToken(w);
	/* TAKE CARE: if the last was not an '=' this is  *
			   * still the last token read                      */
	if (SendTokenBack || (*w!='>'))
	    Warning("expected '=>'");
	    /* This is made because the 'keyword' here is "=>", made by *
	     * 2 short tokens                                           */

	/* Get in 'tmp' the actual terminal */
	GetName(tmp);
	if (output) {
	    (void) strcpy(first.actual,tmp);
	    alim=1;
	    output=0;
	} else {
	    if (clk) {
		if (clock[0]!='\0')
		    Warning("Multiple clock lines in this gate");
		(void) strcpy(clock,tmp);
		alim=1 ;
		clk=0;
	    } else {
		(void) strcpy(ptr->actual,tmp);
	    };
	};

	GetNextToken(w);
	if (*w==')') break;
	if (*w!=',') {
	    VstError("expected ',' or ')'",";");
	    err=1;
	    SendTokenBack=1;
	    break;
	};
    } while(1);

    /* Ond of connections, these are the finals checks */
    GetNextToken(w);
    if (*w!=';') Warning("expected ';'");

    if (first.formal[0]=='\0') {
	(void) sprintf(tmp,"*Error* Line %u : No outputs given, could *NOT* continue",line);
	Error(tmp);
    };

    if (err) {
	/* ther was an error so print it... */
	/* ok, it's nonsense..              */
	(void) fprintf(Out," # error in source ...\n");
    } else {
	/* ok now let's print'em ordered... */
	for(ptr=first.next; ; ptr=ptr->next) {
	    if (alim && (ptr->next==NULL)) break;
	    if (ptr==NULL) break;
	    (void) fprintf(Out,"%s=%s ",ptr->formal,ptr->actual);
	};
	/* the output is the last */
	(void) fprintf(Out,"%s=%s ",first.formal,first.actual);
	if (!gate)
	    (void) fprintf(Out,"%s %c",clock,INIT);
    };

    /* Now let's check if all is connected */
    if (!AllUsed(ThisCell)){
	(void) fprintf(stderr,"*Error* Line %u : Not all formals connected in %s\n",line,ThisCell->name);
	(void) fprintf(Out," # Not all formal connected \n");
    };

    (void) fprintf(Out,"\n");
    FreeConns(&first);
}

/* -=[ GetArchitecture ]=-                    *
 * parses the structure 'ARCHITECTURE'        */
void GetArchitecture()
{
    char *w;
    char LocalToken[MAXTOKENLEN];
    char name[MAXTOKENLEN];
    char msg[MAXTOKENLEN];

    w= &(LocalToken[0]);
    /* type of architecture... */
    GetNextToken(w);

    GetNextToken(w);
    if (!KwrdCmp(w,"OF"))
	Warning("expected syntax: ARCHITECTURE <type> OF <name> IS");
    GetNextToken(w);
    (void) strcpy(name,w);
    GetNextToken(w);
    if (!KwrdCmp(w,"IS"))
	Warning("expected syntax: ENTITY <name> IS");

    /* Components and signals: before a 'BEGIN' only sturcture *
     * COMPONENT and SIGNAL are allowed                        */
    do{
	GetNextToken(w);
	if (KwrdCmp(w,"COMPONENT")){
	    GetComponent();
	} else {
	    if (KwrdCmp(w,"SIGNAL")){
		GetSignal();
	    } else {
		if (KwrdCmp(w,"BEGIN")) break;
		else {
		    (void) sprintf(msg,"%s unknown, skipped",w);
		    Warning(msg);
		    SendTokenBack=0; /* as we said we must skip it */
		};
	    };
	};
	if (feof(In)) Error("Unespected EoF");
    } while(1);

    /* NETLIST */
    do{
	GetNextToken(w);
	if (KwrdCmp(w,"END")) {
	    break;
	} else {
	    /* there's no need to remember the name given to the  *
	     * instance, it's needed only the component name      */
	    GetInstance();
	};
	if (feof(In)) Error("Unespected EoF");
    } while(1);

    /* name of kind of architecture */
    GetNextToken(w);
    /* End of architecture last ';'*/
    GetNextToken(w);
    if (*w!=';') Warning("extected ';'");
};

/* -=[ PARSE FILE ]=-                               *
 * switches between the two main states of          *
 * the program : the ENTITY prsing and the          *
 * ARCHITECTURE one.                                */  
void ParseFile()
{
    char *w;
    char LocalToken[MAXTOKENLEN];
    int flag;
    int entity;


    w= &(LocalToken[0]);
    (void) fprintf(Out,"# File created by vst2blif ver 1.0\n");
    (void) fprintf(Out,"#        by Roberto Rambaldi \n");
    (void) fprintf(Out,"#   D.E.I.S. Universita' di Bologna\n");
    do {

	/* ENTITY CLAUSE */
	flag=0;
	entity=0;
	do {
	    GetNextToken(w);
	    if ((flag=KwrdCmp(w,"ENTITY"))) {
		GetEntity();
		entity=1;
	    } else {
		if (*w=='\0') break;
		VstError("No Entity ???","ENTITY");
	        /* After this call surely flag will be true *
		 * in any other cases the program will stop *
		 * so this point will be never reached ...  */
	    };
	} while(!flag);

	/* ARCHITECTURE CLAUSE */
	flag=0;
	do {
	    GetNextToken(w);
	    if ((flag=KwrdCmp(w,"ARCHITECTURE"))){
		GetArchitecture();
	    } else {
		if (*w=='\0') break;
		VstError("No Architecture ???","ARCHITECTURE");
	        /* it's the same as the previous one         */
	    };
	} while (!flag);
	/* end of the model */
	if (entity) (void) fprintf(Out,"\n.end\n");

    } while (!feof(In));

};

/* -=[ main ]=-                                     */
int main(argc,argv)
int  argc;
char **argv;
{

    CheckArgs(argc,argv);

    ParseFile();

    CloseAll();

    exit(0);
};

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>
#include <ctype.h>
#include <math.h>
#include "score.h"

#define MIN(a,b) ((a)<(b)?(a):(b))

void sort_scores(float *scores,int f,int l);

static void swap(float *scores,int s1, int s2);
static float normalise_score(float score,float n,float ntot,float ntotseq);
static void mk_a_group(int seed,float cutoff,int nseqs,int *seqgroup,int *grouplen,int *groupseed,float **pcid);

static void group_seqs(IN_TREEPTR itree,INODEPTR p, sint *next_groups, sint *nsets, sint **sets);
static void mark_group1(INODEPTR p, sint *groups, IN_TREEPTR itree);
static void mark_group2(INODEPTR p, sint *groups, IN_TREEPTR itree);

extern char **names;
extern char **seq_array;
extern int *seqlen_array;
extern int seqlength;
extern int nseqs;
extern int *useqlen_array;
extern int *use_seq;

extern float **gop;
extern float **gep;

extern int ngroups;
extern int norphans;
extern Boolean *fragment;

void *ckalloc(size_t bytes)
{
	register void *ret;
	
	if( (ret = calloc(bytes, sizeof(char))) == NULL)
		fatal("Out of memory\n");
	else
		return ret;	

	return ret;	
}

/*
*	ckrealloc()
*
*	Tries to reallocate "bytes" bytes of memory. Exits program if failed.
*	Return value:
*		Generic pointer to the re-allocated memory.
*/

void *ckrealloc(void *ptr, size_t bytes)
{
	register void *ret=NULL;

	if (ptr == NULL)	
		fatal("Bad call to ckrealloc\n");
	else if( (ret = realloc(ptr, bytes)) == NULL)
		fatal("Out of memory\n");
	else
		return ret;	

	return ret;	
}

/*
*	ckfree()
*
*	Tries to free memory allocated by ckalloc.
*	Return value:
*		None.
*/

void *ckfree(void *ptr)
{
	if (ptr == NULL)
		warning("Bad call to ckfree\n");
	else {
	 	free(ptr);
		ptr = NULL;
	}
	return ptr;
}


/*
*	rtrim()
*
*	Removes trailing blanks from a string
*
*	Return values:
*		Pointer to the processed string
*/

char * rtrim(char *str)
{
	register int p;

	p = strlen(str) - 1;
	
	while ( isspace(str[p]) )
		p--;
		
	str[p + 1] = EOS;
	
	return str;
}


/*
*	blank_to_()
*
*	Replace blanks in a string with underscores
*
*       Also replaces , ; : ( or ) with _
*
*	Return value:
*		Pointer to the processed string
*/

char * blank_to_(char *str)
{
	int i,p;

	p = strlen(str);
	
	for(i=0;i<p;i++) 
		if(
                     (str[i]==' ') ||
                     (str[i]==';') ||
                     (str[i]==',') ||
                     (str[i]=='(') ||
                     (str[i]==')') ||
                     (str[i]==':')
                  )
                      str[i] = '_';
	
	return str;
}


/*
*	upstr()
*
*	Converts string str to uppercase.
*	Return values:
*		Pointer to the converted string.
*/

char * upstr(char *str)
{
	register char *s = str;
	
	while( (*s = toupper(*s)) )
		s++;
		
	return str;
}

/*
*	lowstr()
*
*	Converts string str to lower case.
*	Return values:
*		Pointer to the converted string.
*/

char * lowstr(char *str)
{
	sint i,len;

	len=strlen(str);
	for(i=0;i<len;i++) {
		if(isupper(str[i])) str[i] = tolower(str[i]);
	}
		
	return str;
}

void getstr(char *instr,char *outstr)
{	
	fprintf(stdout,"%s: ",instr);
	gets(outstr);
}

double getreal(char *instr,double minx,double maxx,double def)
{
	int status;
	float ret;
	char line[MAXLINE];	
	
	while(TRUE) {
		fprintf(stdout,"%s (%.1f-%.1f)   [%.1f]: ",instr,minx,maxx,def);
		gets(line);
		status=sscanf(line,"%f",&ret);
		if(status == EOF) return def;
		if(ret>maxx) {
			fprintf(stdout,"ERROR: Max. value=%.1f\n\n",maxx);
			continue;
		}
		if(ret<minx) {
			fprintf(stdout,"ERROR: Min. value=%.1f\n\n",minx);
			continue;
		}
		break;
	}
	return (double)ret;
}


int getint(char *instr,int minx,int maxx, int def)
{
	int ret,status;
	char line[MAXLINE];	

	while(TRUE) {
		fprintf(stdout,"%s (%d..%d)    [%d]: ",
		instr,(pint)minx,(pint)maxx,(pint)def);
		gets(line);
		status=sscanf(line,"%d",&ret);
		if(status == EOF) return def;
		if(ret>maxx) {
			fprintf(stdout,"ERROR: Max. value=%d\n\n",(pint)maxx);
			continue;
		}
		if(ret<minx) {
			fprintf(stdout,"ERROR: Min. value=%d\n\n",(pint)minx);
			continue;
		}
		break;
	}
	return ret;
}

void do_system(void)
{
	char line[MAXLINE];
	
	getstr("\n\nEnter system command",line);
	if(*line != EOS)
		system(line);
	fprintf(stdout,"\n\n");
}


Boolean linetype(char *line,char *code)
{
	return( strncmp(line,code,strlen(code)) == 0 );
}

Boolean keyword(char *line,char *code)
{
	int i,j;
	char key[MAXLINE];

	for(i=0;isspace(line[i]) && line[i]!=EOS;i++);
	for(j=0;!isspace(line[i]) && line[i]!=EOS;i++)
		key[j++]=line[i];
	key[j]=EOS;
	return( strcmp(key,code) == 0 );
}

Boolean blankline(char *line)
{
	int i;

	for(i=0;line[i]!='\n' && line[i]!=EOS;i++) {
		/*if( isdigit(line[i]) ||*/
		if( 
		    isspace(line[i]) ||
		    (line[i] == '*') ||
		    (line[i] == ':') ||
                    (line[i] == '.')) 
			;
		else
			return FALSE;
	}
	return TRUE;
}

void alloc_aln(sint nseqs,ALNPTR mult_aln)
{
	sint i,j;

	mult_aln->seqs = (SEQ *)ckalloc((nseqs+1) * sizeof (SEQ));
	mult_aln->ft = (FT *)ckalloc((nseqs+1) * sizeof (FT));
	mult_aln->repeat = (REP *)ckalloc((nseqs+1) * sizeof (REP));
	mult_aln->go = (GO *)ckalloc((nseqs+1) * sizeof (GO));
	for(i=0;i<nseqs;i++) {
		mult_aln->seqs[i].name = (char *)ckalloc((MAXNAMES+1) * sizeof (char));
		mult_aln->seqs[i].access = (char *)ckalloc((MAXNAMES+1) * sizeof (char));
		mult_aln->seqs[i].nid = (char *)ckalloc((MAXNAMES+1) * sizeof (char));
		mult_aln->seqs[i].title = (char *)ckalloc((MAXTITLES+1) * sizeof (char));
		mult_aln->seqs[i].org = (char *)ckalloc((MAXORGANISMS+1) * sizeof (char));
                mult_aln->seqs[i].data=NULL;
                mult_aln->seqs[i].mask=NULL;
		mult_aln->seqs[i].weight=100;
		mult_aln->seqs[i].len=0;
		mult_aln->seqs[i].sense=0;
		mult_aln->seqs[i].simgroup=0;
		for(j=0;j<MAXFTTYPE;j++)
			mult_aln->ft[i].nentries[j]=0;
		mult_aln->repeat[i].nrepeats=0;
		mult_aln->go[i].ngorefs=0;
	}

	mult_aln->motifs = NULL;
	mult_aln->groups.ngroups = 0;

        mult_aln->nseqs=0;
        mult_aln->ncol_scores=0;
        mult_aln->nanchors=0;
        mult_aln->dnaflag=FALSE;
        strcpy(mult_aln->alphabet,"ABCDEFGHIJKLMNOPQRSTUVWXYZ");
        mult_aln->prf1.nseqs=0;
        mult_aln->prf2.nseqs=0;
        mult_aln->treename[0]='\0';
        mult_aln->prf1.treename[0]='\0';
        mult_aln->prf2.treename[0]='\0';

}

void realloc_aln(sint first_seq,sint nseqs,ALNPTR mult_aln)
{
	sint i,j;

	mult_aln->seqs = (SEQ *)ckrealloc(mult_aln->seqs,(first_seq+nseqs+1) * sizeof (SEQ));
	mult_aln->ft = (FT *)ckrealloc(mult_aln->ft,(first_seq+nseqs+1) * sizeof (FT));
	mult_aln->repeat = (REP *)ckrealloc(mult_aln->repeat,(first_seq+nseqs+1) * sizeof (REP));
	mult_aln->go = (GO *)ckrealloc(mult_aln->go,(first_seq+nseqs+1) * sizeof (GO));
	for(i=first_seq;i<first_seq+nseqs;i++) {
                mult_aln->seqs[i].name = (char *)ckalloc((MAXNAMES+1) * sizeof (char));
		mult_aln->seqs[i].access = (char *)ckalloc((MAXNAMES+1) * sizeof (char));
		mult_aln->seqs[i].nid = (char *)ckalloc((MAXNAMES+1) * sizeof (char));
                mult_aln->seqs[i].title = (char *)ckalloc((MAXTITLES+1) * sizeof (char));
                mult_aln->seqs[i].org = (char *)ckalloc((MAXORGANISMS+1) * sizeof (char));
                mult_aln->seqs[i].data=NULL;
                mult_aln->seqs[i].mask=NULL;
                mult_aln->seqs[i].weight=100;
                mult_aln->seqs[i].len=0;
                mult_aln->seqs[i].simgroup=0;
		for(j=0;j<MAXFTTYPE;j++)
			mult_aln->ft[i].nentries[j]=0;
		mult_aln->repeat[i].nrepeats=0;
		mult_aln->go[i].ngorefs=0;
        }
}

void free_aln(ALNPTR mult_aln)
{
	sint i,j,k;

	if(mult_aln->nseqs<=0) return;

        for(i=0;i<mult_aln->nseqs;i++) {
		ckfree(mult_aln->seqs[i].name);
		ckfree(mult_aln->seqs[i].title);
		ckfree(mult_aln->seqs[i].org);
		ckfree(mult_aln->seqs[i].data);
		ckfree(mult_aln->seqs[i].mask);
        	for(j=0;j<MAXFTTYPE;j++) {
        		for(k=0;k<mult_aln->ft[i].nentries[j];k++) {
				ckfree(mult_aln->ft[i].data[j][k].type);
				ckfree(mult_aln->ft[i].data[j][k].name);
			}
		}
        	for(k=0;k<mult_aln->go[i].ngorefs;k++) {
			ckfree(mult_aln->go[i].goref[k].id);
			ckfree(mult_aln->go[i].goref[k].desc);
		}
	}
	ckfree(mult_aln->seqs);
	ckfree(mult_aln->ft);
	ckfree(mult_aln->repeat);
	ckfree(mult_aln->go);
}

void alloc_seq(SEQ *seq,sint length)
{
	seq->data = (char *)ckalloc((length+2) * sizeof (char));
	seq->mask = (char *)ckalloc((length+2) * sizeof (char));
}

void realloc_seq(SEQ *seq,sint length)
{
	seq->data = (char *)ckrealloc(seq->data, (length+2) * sizeof (char));
	seq->mask = (char *)ckrealloc(seq->mask, (length+2) * sizeof (char));
}

void alloc_ft_entry(FT_ENTRY *data)
{
	data->type = (char *)ckalloc((10+2) * sizeof (char));
	data->name = (char *)ckalloc((100+2) * sizeof (char));
}

void alloc_taxon_entry(char *taxon)
{
        taxon = (char *)ckalloc((100+2) * sizeof (char));
}

void alloc_go_entry(GOREF *goref)
{
	goref->id = (char *)ckalloc((10+2) * sizeof (char));
	goref->desc = (char *)ckalloc((100+2) * sizeof (char));
}

int getargs(char *inline1,char *args[],int max)
{

        char    *inptr;
        int     i;

        inptr=inline1;
        for (i=0;i<=max;i++)
        {
                if ((args[i]=strtok(inptr," \t\n"))==NULL)
                        break;
                inptr=NULL;
        }

        return(i);
}

int getintargs(char *inline1,sint *args,int max)
{

        char    *inptr;
        char    *tstring;
/*
#ifndef MAC
        char    *strtok(char *s1, const char *s2);
#endif
*/
        int     i;

        inptr=inline1;
        for (i=0;i<=max;i++)
        {
                if ((tstring=strtok(inptr," \t\n"))==NULL)
                        break;
                args[i]=atoi(tstring);
                inptr=NULL;
        }

        return(i);
}


/* 
   count the number of identities between two sequences and divide by number of residues aligned
	seq1	first sequence
	seq2	second sequence
*/
float countid(SEQ seq1,SEQ seq2)
{
   char c1,c2;
   sint i;
   sint count,total;
   float score;

   count = total = 0;
   for (i=0;i<seq1.len && i<seq2.len;i++) {
     c1 = seq1.data[i];
     c2 = seq2.data[i];
     if (isalpha(c1) && isalpha(c2)) {
       total++;
       if (c1 == c2) count++;
     }

   }

   if(total==0) score=0;
   else
   score = (float)(100.0 * (float)count / (float)total);
   return(score);

}
/* 
   count the number of identities between two sequences and divide by length of sequence
	seq1	first sequence
	seq2	second sequence
*/
float countid1(SEQ seq1,SEQ seq2)
{
   char c1,c2;
   sint i;
   sint count,total;
   sint len;
   float score;

   len = count = total = 0;
   for (i=0;i<seq1.len && i<seq2.len;i++) {
     c1 = seq1.data[i];
     c2 = seq2.data[i];
     if(isalpha(c1) || isalpha(c2)) len++;
     if (isalpha(c1) && isalpha(c2)) {
       total++;
       if (c1 == c2) count++;
     }

   }

   if(len==0) score=0;
   else
   score = (float)(100.0 * (float)count / (float)len);
   return(score);

}

FILE *open_explicit_file(char *file_name)
{
        FILE * file_handle;

        if (*file_name == EOS) {
                error("Bad output file [%s]",file_name);
                return NULL;
        }
        if((file_handle=fopen(file_name,"w"))==NULL) {
                error("Cannot open output file [%s]",file_name);
                return NULL;
        }
        return file_handle;
}

void remove_gap_pos(ALNPTR mult_aln)
{
        int i,j,k,ngaps;
        int fseq,lseq;

        fseq=0;
        lseq=mult_aln->nseqs;
        if(fseq>=lseq) return;

        for (i=0;i<mult_aln->seqs[fseq].len;)
        {
                ngaps=0;
                for (j=fseq;j<lseq;j++)
                        if(!isalpha(mult_aln->seqs[j].data[i])) ngaps++;
                if (ngaps==lseq-fseq)
                {
                        for (j=fseq;j<lseq;j++)
                        {
                                for(k=i+1;k<=mult_aln->seqs[j].len;k++)
                                        mult_aln->seqs[j].data[k-1]=mult_aln->seqs[j].data[k];
                                mult_aln->seqs[j].len--;
                        }
                        if(mult_aln->seqs[fseq].len<=0) break;
                }
                else i++;
        }
}

void swap_scores(float *scores,int s1, int s2)
{
        float temp;

        temp=scores[s1];
        scores[s1]=scores[s2];
        scores[s2]=temp;
}


/*
*	fatal()
*
*	Prints error msg to stdout and exits.
*	Variadic parameter list can be passed.
*
*	Return values:
*		none
*/

void fatal( char *msg,...)
{
	va_list ap;
	
	va_start(ap,msg);
	fprintf(stdout,"\n\nFATAL ERROR: ");
	vfprintf(stdout,msg,ap);
	fprintf(stdout,"\n\n");
	va_end(ap);
	exit(1);
}

/*
*	error()
*
*	Prints error msg to stdout.
*	Variadic parameter list can be passed.
*
*	Return values:
*		none
*/

void error( char *msg,...)
{
	va_list ap;
	
	va_start(ap,msg);
	fprintf(stdout,"\n\nERROR: ");
	vfprintf(stdout,msg,ap);
	fprintf(stdout,"\n\n");
	va_end(ap);
}

/*
*	warning()
*
*	Prints warning msg to stdout.
*	Variadic parameter list can be passed.
*
*	Return values:
*		none
*/

void warning( char *msg,...)
{
	va_list ap;
	
	va_start(ap,msg);
	fprintf(stdout,"\n\nWARNING: ");
	vfprintf(stdout,msg,ap);
	fprintf(stdout,"\n\n");
	va_end(ap);
}

/*
*	info()
*
*	Prints info msg to stdout.
*	Variadic parameter list can be passed.
*
*	Return values:
*		none
*/

void info( char *msg,...)
{
	va_list ap;
	
	va_start(ap,msg);
	fprintf(stdout,"\n");
	vfprintf(stdout,msg,ap);
	va_end(ap);
}

sint create_sets(sint nseq,sint **sets,IN_TREEPTR itree)
{
  sint *groups;
  sint nsets;

  nsets = 0;
  if (nseq >= 2)
     {
/*
  If there are more than two sequences....
*/
       groups = (sint *)ckalloc((nseq+1) * sizeof(sint));
       group_seqs(itree,itree->root,groups,&nsets,sets);
       groups=ckfree((void *)groups);

     }
  return nsets;

}


static void group_seqs(IN_TREEPTR itree,INODEPTR p, sint *next_groups, sint *nsets, sint **sets)
{
    sint i;
    sint *tmp_groups;

    tmp_groups = (sint *)ckalloc((itree->nleaves+1) * sizeof(sint));
    for (i=0;i<itree->nleaves;i++)
         tmp_groups[i] = 0;

    if (p->left != NULL)
      {
         if (p->left->leaf == NODE)
            {
               group_seqs(itree,p->left, next_groups, nsets,sets);
               for (i=0;i<itree->nleaves;i++)
                 if (next_groups[i] != 0) tmp_groups[i] = 1;
            }
         else
            {
               mark_group1(p->left, tmp_groups, itree);
            }
               
      }

    if (p->right != NULL)
      {
         if (p->right->leaf == NODE)
            {
               group_seqs(itree,p->right, next_groups, nsets,sets);
               for (i=0;i<itree->nleaves;i++)
                    if (next_groups[i] != 0) tmp_groups[i] = 2;
            }
         else 
            {
               mark_group2(p->right, tmp_groups, itree);
            }
    	for (i=0;i<itree->nleaves;i++)
      		sets[*nsets][i] = tmp_groups[i];
    	(*nsets)++;
      }
    for (i=0;i<itree->nleaves;i++)
      next_groups[i] = tmp_groups[i];

    tmp_groups=ckfree((void *)tmp_groups);

}

static void mark_group1(INODEPTR p, sint *groups, IN_TREEPTR itree)
{
    sint i;

    for (i=0;i<itree->nleaves;i++)
       {
         if (itree->leafptr[i] == p)
              groups[i] = 1;
         else
              groups[i] = 0;
       }
}

static void mark_group2(INODEPTR p, sint *groups, IN_TREEPTR itree)
{
    sint i;

    for (i=0;i<itree->nleaves;i++)
       {
         if (itree->leafptr[i] == p)
              groups[i] = 2;
         else if (groups[i] != 0)
              groups[i] = 1;
       }
}

void col2pos(char *seq,sint cstart,sint cend,sint *pstart,sint *pend)
{
        int i,ix;

        ix=0;
        if(cstart<0)
        {
                (*pstart)=-1;
                (*pend)=-1;
                return;
        }
        for(i=0;i<strlen(seq);i++)
        {
                if(isalpha(seq[i])) ix++;
                if(i==cstart) break;
        }
        (*pstart)=ix-1;

        if (cend<=cstart)
        {
                (*pend)=(*pstart);
                return;
        }

        i++;
        for(;i<strlen(seq);i++)
        {
                if(isalpha(seq[i])) ix++;
                if(i==cend) break;
        }
        (*pend)=ix-1;

}

void pos2col(char *seq,sint pstart,sint pend,sint *cstart,sint *cend)
{
        int i,ix;
	sint seqlen;

	seqlen=strlen(seq)-1;
        ix=(-1);
        if(pstart<0)
        {
                (*cstart)=0;
                (*cend)=0;
                return;
        }
        for(i=0;i<seqlen;i++)
        {
                if(isalpha(seq[i])) ix++;
                if(ix==pstart) break;
        }
        (*cstart)=i;

        if (pend<=pstart)
        {
                (*cend)=(*cstart);
                return;
        }

        i++;
        for(;i<seqlen;i++)
        {
                if(isalpha(seq[i])) ix++;
                if(ix==pend) break;
        }
        (*cend)=i;

}


sint check_ft_type(char *ft_type,char *ft_name,sint *type)
{
        sint ret=0;
        sint seq_type=0;

        if (strcmp(ft_type,"STRUCT")==0) {
                seq_type=3;
                (*type)=STRUCT;
                ret=1;
        }
        else if ((strcmp(ft_type,"HELIX")==0) ||
           (strcmp(ft_type,"STRAND")==0)) {
                strcpy(ft_name,ft_type);
                strcpy(ft_type,"STRUCT");
                seq_type=3;
                (*type)=STRUCT;
                ret=1;
        }
        else if ((strcmp(ft_type,"DNA_BIND")==0) ||
           (strcmp(ft_type,"ZN_FING")==0)) {
                strcpy(ft_name,ft_type);
                strcpy(ft_type,"DOMAIN");
                (*type)=SWDOMAIN;
                ret=1;
        }
        else if ((strcmp(ft_type,"NP_BIND")==0) ||
           (strcmp(ft_type,"CA_BIND")==0) ||
           (strcmp(ft_type,"METAL")==0) ||
           (strcmp(ft_type,"CARBOHYD")==0) ||
           (strcmp(ft_type,"DISULFID")==0) ||
           (strcmp(ft_type,"SITE")==0) ||
           (strcmp(ft_type,"ACT_SITE")==0) ||
           (strcmp(ft_type,"BINDING")==0)) { 
                strcpy(ft_name,ft_type);
                strcpy(ft_type,"SITE");
                (*type)=SITE;
                ret=1;
        }
        else if (strcmp(ft_type,"MOD_RES")==0) {
               	(*type)=MODRES;
               	ret=1;
        }
        else if (strcmp(ft_type,"REPEAT")==0) {
                (*type)=REPEAT;
                ret=1;
        }
        else if (strcmp(ft_type,"TRANSMEM")==0) {
                (*type)=TRANSMEM;
                ret=1;
        }
        else if (strcmp(ft_type,"SIGNAL")==0) {
                (*type)=SIGNAL;
                ret=1;
        }
        else if (strcmp(ft_type,"VARSPLIC")==0) {
                (*type)=VARSPLIC;
                ret=1;
        }
        else if (strcmp(ft_type,"VARIANT")==0) {
                (*type)=VARIANT;
                ret=1;
        }
        else if (strcmp(ft_type,"DOMAIN")==0) {
                (*type)=SWDOMAIN;
                ret=1;
        }
        else if (strcmp(ft_type,"BLOCK")==0) {
                (*type)=COREBLOCK;
                ret=1;
        }
        else if (strcmp(ft_type,"REGION")==0) {
                (*type)=REGION;
                ret=1;
        }
        else if (strcmp(ft_type,"PHYLOBLOCK")==0) {
                (*type)=PHYLOBLOCK;
                ret=1;
        }
        else if (strcmp(ft_type,"SEQERR")==0) {
                (*type)=SEQERRBLOCK;
                ret=1;
        }
        else if (strcmp(ft_type,"COIL")==0) {
                (*type)=COIL;
                ret=1;
        }
        else if (strcmp(ft_type,"LOWCOMP")==0) {
                (*type)=LOWC;
                ret=1;
        }
        else if (strcmp(ft_type,"PFAM-A")==0) {
                (*type)=PFAMA;
                ret=1;
        }
        else if (strcmp(ft_type,"PFAM-B")==0) {
                (*type)=PFAMB;
                ret=1;
        }
        else if (strcmp(ft_type,"PROSITE")==0) {
                (*type)=PROSITE;
                ret=1;
        }
        else if (strcmp(ft_type,"ELM")==0) {
                (*type)=ELM;
                ret=1;
        }
        else if (strcmp(ft_type,"DISORDER")==0) {
                (*type)=DISORDER;
                ret=1;
        }
        return ret;
}

void add_ft_entry(ALNPTR mult_aln,sint seq,sint first,sint last,sint type,sint code,float score,char *ctype,char *iname,sint is,sint ie)
{
        sint n,color;
        sint fr,lr;
        sint fc,lc;
	char name[100];

        if(mult_aln->ft[seq].nentries[type]>MAXFT) {
                fprintf(stdout,"WARNING: too many features in %s %d (%d)\n",mult_aln->seqs[seq].name,mult_aln->ft[seq].nentries[type],type);
                return;
        }
        if(last<is || first>ie) return;
        if(first<is) first=is;
        if(last>ie) last=ie;
        n=mult_aln->ft[seq].nentries[type];
        alloc_ft_entry(&mult_aln->ft[seq].data[type][n]);
        strcpy(mult_aln->ft[seq].data[type][n].type,ctype);
        col2pos(mult_aln->seqs[seq].data,first,last,&fr,&lr);
        pos2col(mult_aln->seqs[seq].data,fr,lr,&fc,&lc);
        if(fc==first)
                mult_aln->ft[seq].data[type][n].start=fr;
        else
                mult_aln->ft[seq].data[type][n].start=fr+1;
        if(lc==last)
                mult_aln->ft[seq].data[type][n].end=lr;
        else
                mult_aln->ft[seq].data[type][n].end=lr-1;

	strcpy(name,iname);
	if(strcmp(ctype,"BLOCK")==0) {
		if (strcmp(name,"SBLOCK")==0) 
			color=code;
		else if (strcmp(name,"LBLOCK")==0) 
			color=12;
	}
	else if(strcmp(ctype,"REGION")==0) {
		color=10;
	}
	else if(strcmp(ctype,"SEQERR")==0) {
		color=3;
	}
        else if(strcmp(ctype,"ANCHOR")==0) {
                color=code;
        }
        else if(strcmp(ctype,"REPEAT")==0) {
                color=code;
        }
	else if(strcmp(ctype,"STRUCT")==0) {
		if(strncmp(name,"PRED_HELIX",10)==0) color=VLRED;
		else if(strncmp(name,"PRED_STRAND",11)==0) color=VLGREEN;
		else if(strncmp(name,"PROP_HELIX",10)==0) color=LRED;
		else if(strncmp(name,"PROP_STRAND",11)==11) color=LGREEN;
		else if(strcmp(name,"HELIX")==0) color=RED;
		else if(strcmp(name,"STRAND")==0) color=GREEN;
	}
	else if(strcmp(ctype,"TRANSMEM")==0) {
		if(strncmp(name,"PRED_",5)==0) {
			sprintf(name,"%s %.2f",name,score);
			color=10;
		}
		else if(strncmp(name,"PROP_",5)==0) color=GRAY;
		else color=1;
	}
	else if(strcmp(ctype,"COIL")==0) {
		if(strncmp(name,"PRED_",5)==0) color=LGRAY;
		else if(strncmp(name,"PROP_",5)==0) color=GRAY;
		else color=6;
	}
        else if(strcmp(ctype,"LOWCOMP")==0) {
                if(strncmp(name,"PRED_",5)==0) color=LGRAY;
                else if(strncmp(name,"PROP_",5)==0) color=GRAY;
                else color=6;
        }
        else if(strcmp(ctype,"DOMAIN")==0) {
                if(strncmp(name,"PRED_",5)==0) color=LGRAY;
                else if(strncmp(name,"PROP_",5)==0) color=GRAY;
                else if(strncmp(name,"DNA_BIND",8)==0) color=2;
                else if(strncmp(name,"ZN_FING",7)==0) color=3;
                else color=1;
        }
        else if(strcmp(ctype,"PFAM-A")==0) {
                if(strncmp(name,"PRED_",5)==0) color=LGRAY;
                else if(strncmp(name,"PROP_",5)==0) color=GRAY;
                else color=code;
        }
        else if(strcmp(ctype,"PFAM-B")==0) {
                if(strncmp(name,"PRED_",5)==0) color=LGRAY;
                else if(strncmp(name,"PROP_",5)==0) color=GRAY;
                else color=code;
        }
        else if(strcmp(ctype,"PROSITE")==0) {
                if(strncmp(name,"PRED_",5)==0) color=LGRAY;
                else if(strncmp(name,"PROP_",5)==0) color=GRAY;
                else color=code;
        }
        else if(strcmp(ctype,"SIGNAL")==0) {
                if(strncmp(name,"PRED_",5)==0) {
                        sprintf(name,"%s %.2f",name,score);
                        color=LGRAY;
                }
                else if(strncmp(name,"PROP_",5)==0) color=GRAY;
                else if(strncmp(name,"NUCLEAR",7)==0) color=1;
                else if(strncmp(name,"CHLOROPLAST",11)==0) color=2;
                else if(strncmp(name,"MITOCHONDRION",13)==0) color=3;
                else if(strncmp(name,"MICROBODY",9)==0) color=4;
                else color=5;
        }
        else if(strcmp(ctype,"VARSPLIC")==0) {
                if(strncmp(name,"PRED_",5)==0) color=LGRAY;
                else if(strncmp(name,"PROP_",5)==0) color=GRAY;
                else color=5;
        }
        else if(strcmp(ctype,"MOD_RES")==0) {
                color=0;
                if(strncmp(name,"PRED_",5)==0) color=LGRAY;
                else if(strncmp(name,"PROP_",5)==0) color=GRAY;
                else if(strncmp(name,"ACETYLATION",11)==0) color=1;
                else if(strncmp(name,"ALKYLATION",10)==0) color=2;
                else if(strncmp(name,"HYDROXYLATION",13)==0) color=3;
                else if(strncmp(name,"METHYLATION",11)==0) color=4;
                else if(strncmp(name,"PHOSPHORYLATION",15)==0) color=5;
                else if(strncmp(name,"SULFATION",9)==0) color=6;
                else if(strncmp(name,"PYRROLIDONE",11)==0) color=7;
                else if(strncmp(name,"MYRISTATE",9)==0) color=8;
                else if(strncmp(name,"PALMITATE",9)==0) color=9;
                else if(strncmp(name,"FARNESYL",8)==0) color=10;
                else if(strncmp(name,"GERANYL",7)==0) color=11;
                else if(strncmp(name,"GPI-ANCHOR",3)==0) color=12;
                else if(strncmp(name,"N-ACYL",5)==0) color=13;
        }
        else if(strcmp(ctype,"SITE")==0) {
                color=0;
                if(strncmp(name,"PRED_",5)==0) color=LGRAY;
                else if(strncmp(name,"PROP_",5)==0) color=GRAY;
                else if(strncmp(name,"SITE",4)==0) color=0;
                else if(strncmp(name,"ACT_SITE",8)==0) color=1;
                else if(strncmp(name,"CARBOHYD",8)==0) color=2;
                else if(strncmp(name,"DISULFID",8)==0) color=3;
                else if(strncmp(name,"BINDING",7)==0) color=4;
                else if(strncmp(name,"NP_BIND",7)==0) color=5;
                else if(strncmp(name,"CA_BIND",7)==0) color=6;
                else if(strncmp(name,"TRANSIT",7)==0) color=7;
                else if(strncmp(name,"LIPID",5)==0) color=8;
                else if(strncmp(name,"METAL",5)==0) color=9;
        }

        strcpy(mult_aln->ft[seq].data[type][n].name,name);
        mult_aln->ft[seq].data[type][n].score=score;
        mult_aln->ft[seq].data[type][n].color=color;






        mult_aln->ft[seq].nentries[type]++;

}

float pcidentity(ALN mult_aln,int i,int j)
{
   char c1,c2;
   int k;
   int count, total, len;
   float pcid;

   if(useqlen_array[i]<useqlen_array[j]) len = useqlen_array[i];
   else len = useqlen_array[j];

   count = total = 0;
   for (k=0;k<mult_aln.seqs[i].len;k++) {
     c1 = mult_aln.seqs[i].data[k];
     c2 = mult_aln.seqs[j].data[k];
     if (isalpha((int)c1) && isalpha((int)c2)) {
        total++;
        if (c1 == c2) count++;
     }
   }

   if (total == 0) pcid = 0;
   else pcid = (float)count / (float)total;

   return(pcid);
}


void calc_groups(int seed,float cutoff,int nseqs,float **pcid,int *seqgroup,int *groupseed)
{
        int i,j;
        int *grouplen;

        grouplen=(int *)ckalloc((nseqs+1)*sizeof(int));

        for (i=0;i<nseqs;i++) {
                seqgroup[i]=0;
        }

        ngroups=0;
        norphans=nseqs;

        while (norphans>0) {
                mk_a_group(seed,cutoff,nseqs,seqgroup,grouplen,groupseed,pcid);
/* find the next ungrouped sequence to use as a seed */
                norphans=0;
                for (i=0;i<nseqs;i++)
                        if(use_seq[i]>1 && seqgroup[i]==0) {
                                if (norphans==0) seed=i;
                                norphans++;
                        }
        }

        norphans=0;
        for(i=0;i<ngroups;i++)
        {
                if(grouplen[i]==1)
                        norphans++;
        }

        ckfree(grouplen);
}


void score_gaps(ALN mult_aln,int s1,int s2)
{
	int i,j,k,start,len;
	int is,ie;
   	int count, total, ngaps, gaplen;
   	char c1,c2;
	float score;
	char *seq1,*seq2;
	Boolean in_gap,lin_gap;
 
   	if(mult_aln.seqs[s1].len<mult_aln.seqs[s2].len) len = mult_aln.seqs[s1].len;
   	else len = mult_aln.seqs[s2].len;

/* find the start and end of the pairwise alignment */
	is=0;
	ie=seqlength;

	for(k=0;k<len;k++) {
     		c1 = mult_aln.seqs[s1].data[k];
     		c2 = mult_aln.seqs[s2].data[k];
		if (isalpha(c1) && isalpha(c2)) {
			is=k;
			break;
		}
	}
	for(k=len-1;k>=0;k--) {
     		c1 = mult_aln.seqs[s1].data[k];
     		c2 = mult_aln.seqs[s2].data[k];
		if (isalpha(c1) && isalpha(c2)) {
			ie=k;
			break;
		}
	}

/* remove common gap positions */
	seq1=(char *)ckalloc((len+1) * sizeof(char));
	seq2=(char *)ckalloc((len+1) * sizeof(char));
	len=0;
	for(i=is;i<ie && i<len;i++) {
		if(isalpha(mult_aln.seqs[s1].data[i]) || isalpha(mult_aln.seqs[s2].data[i])) {
			seq1[len]=mult_aln.seqs[s1].data[i];
			seq2[len]=mult_aln.seqs[s2].data[i];
			len++;
		}
	}

	ngaps=gaplen=0;
	lin_gap=in_gap=FALSE;
	for(i=0;i<len;i++)
	{
		if(isalpha(seq1[i]))
		{
			in_gap=FALSE;
		}
		else
		{
			if(in_gap==FALSE) ngaps++;
			gaplen++;
			in_gap=TRUE;
		}
		lin_gap=in_gap;
	}
	lin_gap=in_gap=FALSE;
	for(i=0;i<len;i++)
	{
		if(isalpha(seq2[i]))
		{
			in_gap=FALSE;
		}
		else
		{
			if(in_gap==FALSE) ngaps++;
			gaplen++;
			in_gap=TRUE;
		}
		lin_gap=in_gap;
	}

	/*score=ngaps*go + gaplen*ge;*/

	gop[s1][s2]=gop[s2][s1]=ngaps;
	gep[s1][s2]=gep[s2][s1]=gaplen;

	ckfree(seq1);
	ckfree(seq2);
}


/* calculate the maximum possible md score given the number of sequences and the
estimated average similarity */

float max_score(int nseqs,int len,Boolean *fragment)
{
	char c;
	int k,is,ie;
	float sum;
	int s,p;
	float n,*ntot,ntotseq,score;

	n=0;
	for(s=0;s<nseqs;s++)
		if (use_seq[s]>1) n++;
	if(n<2) return 1.0;
		
	ntotseq=0;
	for(s=0;s<nseqs;s++)
		if (use_seq[s]>1) ntotseq++;

/* make ntot the number of sequences at this position, excluding fragments */
	ntot=(float *)ckalloc((seqlength+1)*sizeof(float));
    	for(p=0; p<seqlength; p++) {
		ntot[p]=0;
	}
	for(s=0;s<nseqs;s++)
		if (use_seq[s]>1) {
			if(fragment[s]) 
				len=useqlen_array[s];
			else
				len=seqlength;
        		is=0;
			ie=len-1;
			for(p=is;p<=ie;p++) ntot[p]++;
		}
	sum=0.0;
	for(p=0;p<seqlength;p++)
	{
		n=0;
    		for(s=0; s<nseqs; s++)
			if(use_seq[s]>1)
				if(p<useqlen_array[s]) n++;
		score=normalise_score(1.0,n,ntot[p],ntotseq);
		sum+=score;
	}
	ckfree(ntot);

	return sum;
}

float md_score(ALNPTR mult_aln,int colix,float matrix[NUMRES][NUMRES],float **weight,float totweight,Boolean *fragment)
{
	char c,c1;
	int i,j,k,s,s1,p,r;
	int len,is,ie;
	int nseqs;
	float *ntot,ntotseq;
	float mean,n,score;
	float dist,diff,sum;
	float seqvector[26],seqvector1[26];
	float resdist[26][26];
	float resfreq[26][26];

	nseqs=mult_aln->nseqs;
	ntotseq=0;
	for(s=0;s<nseqs;s++)
		if (use_seq[s]>1) ntotseq++;

	ntot=(float *)ckalloc((seqlength+1)*sizeof(float));
    	for(p=0; p<seqlength; p++) {
		ntot[p]=0;
	}
	
/* make ntot the number of sequences at this position, excluding fragments */
	for(s=0;s<nseqs;s++)
		if (use_seq[s]>1) {
			len=mult_aln->seqs[s].len;
        		is=0;
			ie=len;
			if(fragment[s]) {
        			for(k=0;k<len;k++) {
                			c = mult_aln->seqs[s].data[k];
                			if (isalpha(c)) {
                        			is=k;
                        			break;
                			}
        			}
        			for(k=len-1;k>=0;k--) {
                			c = mult_aln->seqs[s].data[k];
                			if (isalpha(c)) {
                        			ie=k;
                        			break;
                			}
        			}
			}
			for(p=is;p<=ie;p++) ntot[p]++;
		}

	for(i=0;i<26;i++) {
                for (r=0;r<26; r++)
                        seqvector[r]=matrix[r][i];
                resdist[i][i]=0.0;
                for(j=i+1;j<26;j++) {
                        for (r=0;r<26; r++)
                                seqvector1[r]=matrix[r][j];
                        resdist[i][j]=0.0;
                        for(r=0;r<26;r++) {
                                diff=seqvector1[r]-seqvector[r];
                                resdist[i][j]+=diff*diff;
                        }
                        resdist[i][j]=sqrt((double)resdist[i][j]);
                        resdist[j][i]=resdist[i][j];
                }
        }
	sum=0.0;
    	for(p=0; p<seqlength; p++)
	{
		if(ntot[p]<1) continue;
    		for(i=0;i<26;i++)
    			for(j=0;j<26;j++)
				resfreq[i][j]=0.0;
/* calculate mean of seq distances */
		mean=0.0;
    		for(s=0; s<nseqs; s++)
		{
			if(use_seq[s]>1)
			if(isalpha(mult_aln->seqs[s].data[p])) {
				c=toupper(mult_aln->seqs[s].data[p]);
    				for(s1=s+1; s1<nseqs; s1++)
				{
					if(use_seq[s1]>1)
					if(isalpha(mult_aln->seqs[s1].data[p])) {
						c1=toupper(mult_aln->seqs[s1].data[p]);
						resfreq[c-'A'][c1-'A']+=weight[s][s1];
					}
				}
			}
		}
    		for(i=0;i<26;i++) {
    			for(j=0;j<26;j++) {
				mean+=resdist[i][j]*resfreq[i][j];
			}
		}
		/*mean/=(float)(ntot)*(float)(ntot-1)/2.0;*/
		mean/=totweight;

/* normalise score between 0 and 1 (where 1 is an identical column) */

		score=exp((double)(-mean)/(double)2.0);

/* normalise the score for the number of sequences with residues at this position */
		n=0;
    		for(s=0; s<nseqs; s++)
			if(use_seq[s]>1)
			if(isalpha(mult_aln->seqs[s].data[p]) )
				n++;
		score=normalise_score(score,n,ntot[p],ntotseq);
/*fprintf(stdout,"%d %f %f %f %f\n",p+1,n,ntot[p],ntotseq,score);*/
		mult_aln->col_score[colix].data[p]=100*score;
		sum+=score;
	}
	mult_aln->col_score[colix].length=p;
	mult_aln->col_score[colix].name=(char *)ckalloc(10*sizeof(char));
	mult_aln->col_score[colix].owner=(char *)ckalloc(10*sizeof(char));
	strcpy(mult_aln->col_score[colix].name,"normd_all");
	strcpy(mult_aln->col_score[colix].owner,"all");
	ckfree(ntot);

	return sum;
}

static float normalise_score(float score,float n,float ntot,float ntotseq)
{
	float ret;

	if(n==0) ret=0.0;
	else 
		ret=score*(float)exp((double)(-10.0*(float)(ntot-n)/((float)(ntot))));

	if(ret>1) ret=1.0;
	return ret;

}


void sort_scores(float *scores,int f,int l)
{
        int i,last;

        if(f>=l) return;

        swap(scores,f,(f+l)/2);
        last=f;
        for(i=f+1;i<=l;i++)
        {
                if(scores[i]>scores[f])
                        swap(scores,++last,i);
        }
        swap(scores,f,last);
        sort_scores(scores,f,last-1);
        sort_scores(scores,last+1,l);

}

static void swap(float *scores,int s1, int s2)
{
        float temp;

        temp=scores[s1];
        scores[s1]=scores[s2];
        scores[s2]=temp;
}

static void mk_a_group(int seed,float cutoff,int nseqs,int *seqgroup,int *grouplen,int *groupseed,float **pcid)
{
        int last_time;
        int i,j,k;
        Boolean found;
        int no;

/* the seed sequence becomes the first member of the new group */
        groupseed[ngroups]=seed;
        grouplen[ngroups]=1;
        ngroups++;
        seqgroup[seed]=ngroups;
        norphans--;

        last_time=norphans;
        while(1) {
                no=0;
/* for each sequence that hasn't been grouped yet, check whether it shares high enough percent identity
with a sequence in a group */
                for (i=0;i<nseqs;i++)
                {
                        if(use_seq[i]>1 && seqgroup[i]==0) {
                                found=FALSE;
                                for(k=0;k<nseqs;k++) {
                                        if(use_seq[k]>1 && seqgroup[k]>0 && pcid[i][k]>cutoff) {
                                                grouplen[seqgroup[k]]++;
                                                seqgroup[i]=seqgroup[k];
                                                found=TRUE;
                                                break;
                                        }
                                        if(found==TRUE) break;
                                }
                                if(found==FALSE) no++;
                        }
                }
                if(no==last_time) break;
                last_time=no;
        }
}

void col2pos1(char *seq,sint cstart,sint *pstart)
{
        int i,ix;

        ix=0;
        if(cstart<0)
        {
                (*pstart)=-1;
                return;
        }
        for(i=0;i<strlen(seq);i++)
        {
                if(isalpha(seq[i])) ix++;
                if(i==cstart) break;
        }
        (*pstart)=ix-1;
}

void pos2col1(char *seq,sint pstart,sint *cstart)
{
        int i,ix;

        ix=0;
        if(pstart<0)
        {
                (*cstart)=-1;
                return;
        }
        for(i=0;i<strlen(seq);i++)
        {
                if(isalpha(seq[i])) ix++;
                if(ix==pstart+1) break;
        }
        (*cstart)=i;

}


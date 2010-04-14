/* C prolog +§§§1
   vim:cindent:ft=c:foldmethod=marker:fmr=+§§§,-§§§
   Copyright-Notitz +§§§2 */
/*
 * ##########################################################################
 * #  Dies ist konto_check, ein Programm zum Testen der Prüfziffern         #
 * #  von deutschen Bankkonten. Es kann als eigenständiges Programm         #
 * #  (z.B. mit der beigelegten main() Routine) oder als Library zur        #
 * #  Verwendung in anderen Programmen bzw. Programmiersprachen benutzt     #
 * #  werden.                                                               #
 * #                                                                        #
 * #  Die einleitenden Beschreibungen zu den einzelnen Methoden wurden      #
 * #  wurden aus der aktuellen BLZ-Datei der Deutschen Bundesbank           #
 * #  übernommen.                                                           #
 * #                                                                        #
 * #  Copyright (C) 2002-2009 Michael Plugge <m.plugge@hs-mannheim.de>      #
 * #                                                                        #
 * #  Dieses Programm ist freie Software; Sie dürfen es unter den           #
 * #  Bedingungen der GNU Lesser General Public License, wie von der Free   #
 * #  Software Foundation veröffentlicht, weiterverteilen und/oder          #
 * #  modifizieren; entweder gemäß Version 2.1 der Lizenz oder (nach Ihrer  #
 * #  Option) jeder späteren Version.                                       #
 * #                                                                        #
 * #  Die GNU LGPL ist weniger infektiös als die normale GPL; Code, der von #
 * #  Ihnen hinzugefügt wird, unterliegt nicht der Offenlegungspflicht      #
 * #  (wie bei der normalen GPL); außerdem müssen Programme, die diese      #
 * #  Bibliothek benutzen, nicht (L)GPL lizensiert sein, sondern können     #
 * #  beliebig kommerziell verwertet werden. Die Offenlegung des Sourcecodes#
 * #  bezieht sich bei der LGPL *nur* auf geänderten Bibliothekscode.       #
 * #                                                                        #
 * #  Dieses Programm wird in der Hoffnung weiterverbreitet, daß es         #
 * #  nützlich sein wird, jedoch OHNE IRGENDEINE GARANTIE, auch ohne die    #
 * #  implizierte Garantie der MARKTREIFE oder der VERWENDBARKEIT FÜR       #
 * #  EINEN BESTIMMTEN ZWECK. Mehr Details finden Sie in der GNU Lesser     #
 * #  General Public License.                                               #
 * #                                                                        #
 * #  Sie sollten eine Kopie der GNU Lesser General Public License          #
 * #  zusammen mit diesem Programm erhalten haben; falls nicht,             #
 * #  schreiben Sie an die Free Software Foundation, Inc., 59 Temple        #
 * #  Place, Suite 330, Boston, MA 02111-1307, USA. Sie können sie auch     #
 * #  von                                                                   #
 * #                                                                        #
 * #       http://www.gnu.org/licenses/lgpl.html                            #
 * #                                                                        #
 * # im Internet herunterladen.                                             #
 * ##########################################################################
 */

/* Definitionen und Includes  */
#ifndef VERSION
#define VERSION "3.0"
#endif
#define VERSION_DATE "2009-10-24"

#ifndef INCLUDE_KONTO_CHECK_DE
#define INCLUDE_KONTO_CHECK_DE 1
#endif

#ifndef CHECK_MALLOC
#define CHECK_MALLOC 1
#endif

#define COMPRESS 1   /* Blocks komprimieren */

#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>
#include <limits.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#if COMPRESS>0
#include <zlib.h>
#endif
#if _WIN32>0
#include <windows.h>
#define usleep(t) Sleep(t/1000)
#endif

   /* Debugcode für malloc&Co. */
#if CHECK_MALLOC
typedef struct {
   int is_valid; /* Block aktuell aktiv, nicht freigegeben */
   int cnt;      /* Anzahl mallocs; sollte 0 oder 1 sein, sonst Fehler */
   int zeile;    /* Zeile für malloc */
   int zeile2;   /* Zeile für realloc */
   int size;     /* Größe des Datenblocks */
   int no_name;  /* noch kein Variablenname eingetragen */
   int reported; /* Info zu dem Block wurde bereits im error_log ausgegeben */
   char *name;   /* Variablenname */
   void *ptr;    /* Pointer auf den Datenbereich */
}MALLOC_LISTE;

static int last_malloc_index,malloc_cnt,free_cnt;

#define MALLOC_MAX_CNT 10000

static MALLOC_LISTE malloc_liste[MALLOC_MAX_CNT];

int malloc_entry_cnt(void)
{
   int i,cnt;

   for(cnt=i=0;i<=last_malloc_index;i++)if(malloc_liste[i].is_valid)cnt++;
   return cnt;
}

int malloc_entry_cnt_new(void)
{
   int i,cnt;

   for(cnt=i=0;i<=last_malloc_index;i++)if(malloc_liste[i].is_valid && !malloc_liste[i].reported)cnt++;
   return cnt;
}

static char 
*f1="Zeit: %s, Anzahl noch allokierter Speicherblocks: %d\nAnzahl malloc/calloc-Aufrufe: %d, Anzahl Speicherfreigaben: %d\n",
*f2=" idx  cnt   zeile   zeile2     size            name        ptr\n",
*f3="%4d %4d %7d  %7d %8d %15s 0x%08x\n";

void malloc_report(int mark)
{
   char zeitbuffer[64];
   time_t timep;
   struct tm *zeit;
   int i,cnt;

   time(&timep);
   zeit=localtime(&timep);
   strftime(zeitbuffer,64,"%Y-%m-%d %T",zeit);
   fprintf(stderr,f1,zeitbuffer,cnt=malloc_entry_cnt(),malloc_cnt,free_cnt);
   if(cnt){
      fprintf(stderr,f2);
      for(i=0;i<last_malloc_index;i++)if(malloc_liste[i].is_valid){
         if(mark)malloc_liste[i].reported=1;
         fprintf(stderr,f3,
            i+1,malloc_liste[i].cnt,malloc_liste[i].zeile,malloc_liste[i].zeile2,malloc_liste[i].size,
            malloc_liste[i].no_name?"---":malloc_liste[i].name,malloc_liste[i].ptr);
      }
      fputc('\n',stderr);
   }
}

void xfree(void)
{
   int i,cnt;

   if((cnt=malloc_entry_cnt())){
      for(i=0;i<last_malloc_index;i++)if(malloc_liste[i].is_valid){
         free(malloc_liste[i].ptr);
         malloc_liste[i].is_valid=0;
         fprintf(stderr,"Freigabe (extra): %08x: %d Byte aus Zeile %d/%d\n",(unsigned int)malloc_liste[i].ptr,malloc_liste[i].size,malloc_liste[i].zeile,malloc_liste[i].zeile2);
      }
   }
}

char *malloc_report_string(int mark)
{
   char zeitbuffer[64];
   time_t timep;
   struct tm *zeit;
   static char buffer[65536],*ptr;
   int i,cnt;

   time(&timep);
   zeit=localtime(&timep);
   strftime(zeitbuffer,60,"%Y-%m-%d %T",zeit);
   sprintf(ptr=buffer,f1,zeitbuffer,cnt=malloc_entry_cnt(),malloc_cnt,free_cnt);
   while(*ptr)ptr++;
   if(cnt){
      sprintf(ptr,f2);
      for(i=0;i<last_malloc_index;i++)if(malloc_liste[i].is_valid){
         if(mark)malloc_liste[i].reported=1;
         while(*ptr && ptr<buffer+65000)ptr++;
         sprintf(ptr,f3,i+1,malloc_liste[i].cnt,malloc_liste[i].zeile,malloc_liste[i].zeile2,malloc_liste[i].size,
               malloc_liste[i].no_name?"---":malloc_liste[i].name,malloc_liste[i].ptr);
      }
   }
   return buffer;
}

static void remove_malloc_entry(void *ptr,int zeile)
{
   int i;

   for(i=0;malloc_liste[i].ptr!=ptr && i<MALLOC_MAX_CNT;i++);   /* benutzten Slot suchen */
   if(i<MALLOC_MAX_CNT && malloc_liste[i].cnt==1){
      malloc_liste[i].size=malloc_liste[i].cnt=malloc_liste[i].is_valid=0;
      malloc_liste[i].name=NULL;
      malloc_liste[i].ptr=NULL;
   }
   else{    /* benutzer Slot nicht gefunden; ungültige Freigabe eintragen */
      for(i=0;i<MALLOC_MAX_CNT && !(malloc_liste[i].is_valid || malloc_liste[i].cnt>1);i++); /* freien Slot suchen */
      if(i<MALLOC_MAX_CNT){
         malloc_liste[i].is_valid=-1;
         malloc_liste[i].cnt=1;
         malloc_liste[i].name="???";
         malloc_liste[i].zeile=zeile;
         malloc_liste[i].zeile2=0;
         malloc_liste[i].ptr=ptr;
         if(last_malloc_index<i)last_malloc_index=i;
      }
   }
}

static void add_malloc_ptr(void *ptr,size_t size,int zeile)
{
   int i;

   for(i=0;i<MALLOC_MAX_CNT && (malloc_liste[i].is_valid || malloc_liste[i].cnt>1);i++);       /* freien Slot suchen */
   if(i<MALLOC_MAX_CNT){
      malloc_liste[i].is_valid=1;
      malloc_liste[i].cnt=1;
      malloc_liste[i].name=NULL;
      malloc_liste[i].no_name=1;
      malloc_liste[i].zeile=zeile;
      malloc_liste[i].size=size;
      malloc_liste[i].ptr=ptr;
      if(last_malloc_index<i)last_malloc_index=i;
   }
}

static void set_malloc_ptr(void *old,void *neu,size_t size,int zeile)
{
   int i;

   for(i=0;i<MALLOC_MAX_CNT && malloc_liste[i].ptr!=old;i++);       /* Slot des chunks suchen */
   if(i<MALLOC_MAX_CNT){
      malloc_liste[i].is_valid=1;
      malloc_liste[i].cnt=1;
      malloc_liste[i].zeile2=zeile;
      malloc_liste[i].size=size;
      malloc_liste[i].ptr=neu;
      if(last_malloc_index<i)last_malloc_index=i;
   }
}

static int add_malloc_name(char *name,void *ptr)
{
   int i;

   for(i=0;i<MALLOC_MAX_CNT && malloc_liste[i].ptr!=ptr;i++);       /* Slot suchen */
   if(ptr==malloc_liste[i].ptr){
      malloc_liste[i].no_name=0;
      malloc_liste[i].name=name;
      if(last_malloc_index<i)last_malloc_index=i;
   }
   return i;
}

void m_free(void *ptr,int line)
{
   remove_malloc_entry(ptr,line);
   free(ptr);
   free_cnt++;
}

void *m_realloc(void *ptr,size_t size,int line)
{
   void *old_ptr;

   old_ptr=ptr;
   ptr=realloc(ptr,size);
   set_malloc_ptr(old_ptr,ptr,size,line);
   return ptr;
}

void *m_calloc(size_t nmemb, size_t size,int line)
{
   void *ptr;

   ptr=calloc(nmemb,size);
   add_malloc_ptr(ptr,size*nmemb,line);
   malloc_cnt++;
   return ptr;
}

void *m_malloc(size_t size,int line)
{
   void *ptr;

   ptr=malloc(size);
   add_malloc_ptr(ptr,size,line);
   malloc_cnt++;
   return ptr;
}

   /* für die Buchhaltung alle malloc-Routinen umbiegen auf eigene Routinen */
#define calloc(nmemb,size) m_calloc(nmemb,size,__LINE__)
#define malloc(size)       m_malloc(size,__LINE__)
#define free(ptr)          m_free(ptr,__LINE__)
#define realloc(ptr,size)  m_realloc(ptr,size,__LINE__)
#endif   /* CHECK_MALLOC */

#if 0
   /* RBI: Aufrufe von read_lut_block_int() protokollieren */
#define RBI(var) fprintf(stderr,"==> %08X RBI in Zeile %d von %s: %08X\n",var,__LINE__,__FILE__,var);
#define RBIX(var,typ) fprintf(stderr,"==> %08X RBI in Zeile %d von %s: %08X (typ %d/%s)\n",var,__LINE__,__FILE__,var,typ,lut2_feld_namen[typ]);
//#define FREE(v) do{if(v){free(v); fprintf(stderr,"==> %08X FREE(%08x) in Zeile %d von %s\n",v,v,__LINE__,__FILE__); v=NULL;}}while(0)
#define FREE(v) do{if(v)free(v); v=NULL;}while(0)
#else
#define RBI(var)
#define RBIX(var,typ)
#define FREE(v) do{if(v)free(v); v=NULL;}while(0)
#endif

#if INCLUDE_KONTO_CHECK_DE>0
#ifndef O_BINARY
#define O_BINARY 0   /* reserved by dos */
#endif

#if PHP_MALLOC>0
#undef malloc
#undef calloc
#undef realloc
#undef free
#define malloc(cnt) emalloc(cnt)
#define calloc(cnt,num) ecalloc(cnt,num)
#define realloc(ptr,newsize) erealloc(ptr,newsize)
#define free(ptr) efree(ptr)
#endif

#define KONTO_CHECK_VARS
#include "konto_check.h"

   /* Testwert zur Markierung ungültiger Ziffern im BLZ-String (>8 Stellen) */
#define BLZ_FEHLER 100000000

/* Divisions-Makros (ab Version 2.0) +§§§2 */

   /* Makros zur Modulo-Bildung über iterierte Subtraktionen.
    * auf Intel-Hardware ist dies schneller als eine direkte Modulo-Operation;
    * Auf Alpha ist Modulo allerdings schneller (gute FPU).
    */

#ifdef __ALPHA
#   define MOD_11_352   pz%=11
#   define MOD_11_176   pz%=11
#   define MOD_11_88    pz%=11
#   define MOD_11_44    pz%=11
#   define MOD_11_22    pz%=11
#   define MOD_11_11    pz%=11

#   define MOD_10_320   pz%=10
#   define MOD_10_160   pz%=10
#   define MOD_10_80    pz%=10
#   define MOD_10_40    pz%=10
#   define MOD_10_20    pz%=10
#   define MOD_10_10    pz%=10

#   define MOD_9_288    pz%=9
#   define MOD_9_144    pz%=9
#   define MOD_9_72     pz%=9
#   define MOD_9_36     pz%=9
#   define MOD_9_18     pz%=9
#   define MOD_9_9      pz%=9

#   define MOD_7_224    pz%=7
#   define MOD_7_112    pz%=7
#   define MOD_7_56     pz%=7
#   define MOD_7_28     pz%=7
#   define MOD_7_14     pz%=7
#   define MOD_7_7      pz%=7

#   define SUB1_22      p1%=11
#   define SUB1_11      p1%=11
#else
#   define SUB(x) if(pz>=x)pz-=x
#   define MOD_11_352 SUB(352); MOD_11_176
#   define MOD_11_176 SUB(176); MOD_11_88
#   define MOD_11_88  SUB(88);  MOD_11_44
#   define MOD_11_44  SUB(44);  MOD_11_22
#   define MOD_11_22  SUB(22);  MOD_11_11
#   define MOD_11_11  SUB(11)

#   define MOD_10_320 SUB(320); MOD_10_160
#   define MOD_10_160 SUB(160); MOD_10_80
#   define MOD_10_80  SUB(80);  MOD_10_40
#   define MOD_10_40  SUB(40);  MOD_10_20
#   define MOD_10_20  SUB(20);  MOD_10_10
#   define MOD_10_10  SUB(10)

#   define MOD_9_288 SUB(288);  MOD_9_144
#   define MOD_9_144 SUB(144);  MOD_9_72
#   define MOD_9_72  SUB(72);   MOD_9_36
#   define MOD_9_36  SUB(36);   MOD_9_18
#   define MOD_9_18  SUB(18);   MOD_9_9
#   define MOD_9_9   SUB(9)

#   define MOD_7_224 SUB(224);  MOD_7_112
#   define MOD_7_112 SUB(112);  MOD_7_56
#   define MOD_7_56  SUB(56);   MOD_7_28
#   define MOD_7_28  SUB(28);   MOD_7_14
#   define MOD_7_14  SUB(14);   MOD_7_7
#   define MOD_7_7   SUB(7)

#   define SUB1(x)   if(p1>=x)p1-=x
#   define SUB1_22   SUB1(22);  SUB1_11
#   define SUB1_11   SUB1(11)
#endif

/*
 * ######################################################################
 * # Anmerkung zur DEBUG-Variante:                                      #
 * # Die Debug-Version enthält einige Dinge, die für Puristen tabu sind #
 * # (z.B. das goto im default-Zweig der case-Anweisung, oder (noch     #
 * # schlimmer) die case-Anweisungen in if/else Strukturen (z.B. in     #
 * # Methode 93). Da der Code jedoch nur für Debugzwecke gedacht ist    #
 * # (Verifizierung der Methoden mit anderen Programmen, Generierung    #
 * # Testkonten) und nicht als Lehrbeispiel, mag es angehen ;-)))       #
 * ######################################################################
 */

/* Testmakros für die Prüfziffermethoden +§§§2 */
#if DEBUG>0
#   define CHECK_PZ3     if(retvals){retvals->pz_pos=3; retvals->pz=pz;} return (*(kto+2)-'0'==pz ? OK : FALSE)
#   define CHECK_PZ6     if(retvals){retvals->pz_pos=6; retvals->pz=pz;} return (*(kto+5)-'0'==pz ? OK : FALSE)
#   define CHECK_PZ7     if(retvals){retvals->pz_pos=7; retvals->pz=pz;} return (*(kto+6)-'0'==pz ? OK : FALSE)
#   define CHECK_PZ8     if(retvals){retvals->pz_pos=8; retvals->pz=pz;} return (*(kto+7)-'0'==pz ? OK : FALSE)
#   define CHECK_PZ9     if(retvals){retvals->pz_pos=9; retvals->pz=pz;} return (*(kto+8)-'0'==pz ? OK : FALSE)
#   define CHECK_PZ10    if(retvals){retvals->pz_pos=10;retvals->pz=pz;} return (*(kto+9)-'0'==pz ? OK : FALSE)
#   define CHECK_PZX7    if(retvals){retvals->pz_pos=7; retvals->pz=pz;} if(*(kto+6)-'0'==pz)return OK; if(untermethode)return FALSE
#   define CHECK_PZX8    if(retvals){retvals->pz_pos=8; retvals->pz=pz;} if(*(kto+7)-'0'==pz)return OK; if(untermethode)return FALSE
#   define CHECK_PZX9    if(retvals){retvals->pz_pos=9; retvals->pz=pz;} if(*(kto+8)-'0'==pz)return OK; if(untermethode)return FALSE
#   define CHECK_PZX10   if(retvals){retvals->pz_pos=10; retvals->pz=pz;} if(*(kto+9)-'0'==pz)return OK; if(untermethode)return FALSE
#   define INVALID_PZ10  if(retvals){retvals->pz_pos=10; retvals->pz=pz;} if(pz==10)return INVALID_KTO
#else
#   define CHECK_PZ3     return (*(kto+2)-'0'==pz ? OK : FALSE)
#   define CHECK_PZ6     return (*(kto+5)-'0'==pz ? OK : FALSE)
#   define CHECK_PZ7     return (*(kto+6)-'0'==pz ? OK : FALSE)
#   define CHECK_PZ8     return (*(kto+7)-'0'==pz ? OK : FALSE)
#   define CHECK_PZ9     return (*(kto+8)-'0'==pz ? OK : FALSE)
#   define CHECK_PZ10    return (*(kto+9)-'0'==pz ? OK : FALSE)
#   define CHECK_PZX7    if(*(kto+6)-'0'==pz)return OK
#   define CHECK_PZX8    if(*(kto+7)-'0'==pz)return OK
#   define CHECK_PZX9    if(*(kto+8)-'0'==pz)return OK
#   define CHECK_PZX10   if(*(kto+9)-'0'==pz)return OK
#   define INVALID_PZ10  if(pz==10)return INVALID_KTO
#endif

/* noch einige Makros +§§§2 */
#define EXTRACT(feld) do{ \
   for(sptr=zeile+feld.pos-1,dptr=buffer,j=feld.len;j>0;j--)*dptr++= *sptr++; \
   *dptr=0; \
   while(*--dptr==' ')*dptr=0; \
}while(0)

#define WRITE_LONG(var,out) fputc((var)&255,out); fputc(((var)>>8)&255,out); fputc(((var)>>16)&255,out); fputc(((var)>>24)&255,out)
#define READ_LONG(var) var= *uptr+(*(uptr+1)<<8)+(*(uptr+2)<<16)+(*(uptr+3)<<24); uptr+=4
#define ISDIGIT(x) ((x)>='0' && (x)<='9')

   /* CHECK_RETVAL: Makro speziell für die Funktion generate_lut2().
    *
    * Dieses Makro benutzt ein goto, um zum Aufräumteil am Ende der Funktion zu
    * springen - so ist es einfacher und übersichtlicher, als zu versuchen, die
    * GOTOs durch abenteuerliche (und umständliche) Konstruktionen zu vermeiden.
    */
#define CHECK_RETVAL(fkt) do{if((retval=fkt)!=OK)goto fini;}while(0)     /* es muß noch aufgeräumt werden, daher goto */
#define CHECK_RETURN(fkt) do{if((retval=fkt)!=OK)return retval;}while(0)

   /* einige Makros zur Umwandlung zwischen unsigned int und char */
#define UCP (unsigned char*)
#define SCP (char*)
#define ULP (unsigned long *)
#define UI  (unsigned int)(unsigned char)

#define C2UI(x,p) do{x=((unsigned char)*p)+((unsigned char)*(p+1))*0x100UL; p+=2;} while(0)
#define UI2C(x,p) do{*p++=(char)(x)&0xff; *p++=(char)((x)>>8)&0xff;} while(0)

#define C2UM(x,p) do{x=((unsigned char)*p)+((unsigned char)*(p+1))*0x100UL+((unsigned char)*(p+2))*0x10000UL; p+=3;} while(0)
#define UM2C(x,p) do{*p++=(char)(x)&0xff; *p++=(char)((x)>>8)&0xff; *p++=(char)((x)>>16)&0xff;} while(0)

#define C2UL(x,p) do{x=((unsigned char)*p+((unsigned char)*(p+1))*0x100UL \
   +((unsigned char)*(p+2))*0x10000UL+((unsigned char)*(p+3))*0x1000000UL); p+=4;} while(0)
#define UL2C(x,p) do{*p++=(char)(x)&0xff; *p++=(char)((x)/0x100L)&0xff; \
   *p++=(char)((x)/0x10000L)&0xff; *p++=(char)((x)/0x1000000L)&0xff;} while(0)

#define Z(a,b) a[(int)*(zptr+b)]
#define INVALID_C(ret) {if(retval)*retval=ret; return "";}
#define INVALID_I(ret) {if(retval)*retval=ret; return 0;}

   /* falls eine Initialisierung läuft, warten und jede Millisekunde nachsehen,
    * ob sie fertig ist; nach 10 ms mit Fehlermeldung zurück.
    */
#define INITIALIZE_WAIT do{if(init_in_progress){int i; for(i=0;init_in_progress && i<10;i++)usleep(1000); if(i==10)return INIT_FATAL_ERROR;}} while(0)

   /* noch einige Sachen für LUT2 */
#define SET_OFFSET 100  /* (Typ-)Offset zum zweiten Datensatz der LUT-Datei */
#define MAX_SLOTS 500
#define SLOT_BUFFER (MAX_SLOTS*10+10)
#define CHECK_OFFSET_S do{if(zweigstelle<0 || (filialen && zweigstelle>=filialen[idx]) || (zweigstelle && !filialen)){if(retval)*retval=LUT2_INDEX_OUT_OF_RANGE; return "";} if(retval)*retval=OK;} while(0) 
#define CHECK_OFFSET_I do{if(zweigstelle<0 || (filialen && zweigstelle>=filialen[idx]) || (zweigstelle && !filialen)){if(retval)*retval=LUT2_INDEX_OUT_OF_RANGE; return 0;} if(retval)*retval=OK;} while(0)  


/* globale Variablen +§§§2 */

   /* einige Variablen zur LUT2-Initialisierung und für den LUT-Dump (nur
    * Beispielssets). Immer dabei sind Infoblock, BLZ und Prüfziffer; sie
    * werden daher nicht aufgeführt. 
    */
DLL_EXPORT_V int
    lut_set_0[]={0}, /* 3 Slots */
    lut_set_1[]={LUT2_NAME_KURZ,0}, /* 4 Slots */
    lut_set_2[]={LUT2_NAME_KURZ,LUT2_BIC,0}, /* 5 Slots */
    lut_set_3[]={LUT2_NAME,LUT2_PLZ,LUT2_ORT,0}, /* 6 Slots */
    lut_set_4[]={LUT2_NAME,LUT2_PLZ,LUT2_ORT,LUT2_BIC,0}, /* 7 Slots */
    lut_set_5[]={LUT2_NAME_NAME_KURZ,LUT2_PLZ,LUT2_ORT,LUT2_BIC,0}, /* 7 Slots */
    lut_set_6[]={LUT2_NAME_NAME_KURZ,LUT2_PLZ,LUT2_ORT,LUT2_BIC,LUT2_NACHFOLGE_BLZ,0}, /* 8 Slots */
    lut_set_7[]={LUT2_NAME_NAME_KURZ,LUT2_PLZ,LUT2_ORT,LUT2_BIC,LUT2_NACHFOLGE_BLZ,LUT2_AENDERUNG,0}, /* 9 Slots */
    lut_set_8[]={LUT2_NAME_NAME_KURZ,LUT2_PLZ,LUT2_ORT,LUT2_BIC,LUT2_NACHFOLGE_BLZ,LUT2_AENDERUNG,LUT2_LOESCHUNG,0}, /* 10 Slots */
    lut_set_9[]={LUT2_NAME_NAME_KURZ,LUT2_PLZ,LUT2_ORT,LUT2_BIC,LUT2_NACHFOLGE_BLZ,LUT2_AENDERUNG,LUT2_LOESCHUNG,LUT2_PAN,LUT2_NR,0}, /* 12 Slots */

    lut_set_o0[]={LUT2_BLZ,LUT2_PZ,0},
    lut_set_o1[]={LUT2_BLZ,LUT2_PZ,LUT2_NAME_KURZ,0},
    lut_set_o2[]={LUT2_BLZ,LUT2_PZ,LUT2_NAME_KURZ,LUT2_BIC,0},
    lut_set_o3[]={LUT2_BLZ,LUT2_PZ,LUT2_NAME,LUT2_PLZ,LUT2_ORT,0},
    lut_set_o4[]={LUT2_BLZ,LUT2_PZ,LUT2_NAME,LUT2_PLZ,LUT2_ORT,LUT2_BIC,0},
    lut_set_o5[]={LUT2_BLZ,LUT2_PZ,LUT2_NAME_NAME_KURZ,LUT2_PLZ,LUT2_ORT,LUT2_BIC,0},
    lut_set_o6[]={LUT2_BLZ,LUT2_PZ,LUT2_NAME_NAME_KURZ,LUT2_PLZ,LUT2_ORT,LUT2_BIC,LUT2_NACHFOLGE_BLZ,0},
    lut_set_o7[]={LUT2_BLZ,LUT2_PZ,LUT2_NAME_NAME_KURZ,LUT2_PLZ,LUT2_ORT,LUT2_BIC,LUT2_NACHFOLGE_BLZ,LUT2_AENDERUNG,0},
    lut_set_o8[]={LUT2_BLZ,LUT2_PZ,LUT2_NAME_NAME_KURZ,LUT2_PLZ,LUT2_ORT,LUT2_BIC,LUT2_NACHFOLGE_BLZ,LUT2_AENDERUNG,LUT2_LOESCHUNG,0},
    lut_set_o9[]={LUT2_BLZ,LUT2_PZ,LUT2_NAME_NAME_KURZ,LUT2_PLZ,LUT2_ORT,LUT2_BIC,LUT2_NACHFOLGE_BLZ,LUT2_AENDERUNG,LUT2_LOESCHUNG,LUT2_PAN,LUT2_NR,0};

#if DEBUG>0
   /* falls die Variable verbose_debug gesetzt wird, werden zusätzliche
    * Debuginfos ausgegeben (vor allem für den Perl smoke test gedacht).
    * Das Setzen der Variable erfolgt durch die Funktion set_verbose_debug().
    *
    * Momentan wird diese Funktionalität nicht mehr benutzt, ist aber im Code
    * gelassen, um evl. später reaktiviert zu werden.
    */
static int verbose_debug;
#endif

/* (alte) globale Variablen der externen Schnittstelle +§§§2 */
/*
 * ######################################################################
 * # (alte) globale Variablen der externen Schnittstelle                #
 * # die folgenden globalen Variablen waren in Version 1 und 2 von      #
 * # konto_check definiert; ab Version 3 werden sie nicht mehr unter-   #
 * # stützt. Zur Vermeidung von Linker-Fehlermeldungen können jedoch    #
 * # Dummyversionen eingebunden werden (ohne Funktionalität).           #
 * ######################################################################
 */

#if INCLUDE_DUMMY_GLOBALS>0
DLL_EXPORT_V char *kto_check_msg="Die Variable kto_check_msg wird nicht mehr unterstützt; bitte kto_check_retval2txt() benutzen";
DLL_EXPORT_V char pz_str[]="Die Variable pz_str wird nicht mehr unterstützt; bitte das neue Interface benutzen";
DLL_EXPORT_V int pz_methode=-777;

#if DEBUG>0
DLL_EXPORT
#endif
int pz=-777; 
#endif   /* INCLUDE_DUMMY_GLOBALS */

/* interne Funktionen und Variablen +§§§2 */
/*
 * ######################################################################
 * #               interne Funktionen und Variablen                     #
 * ######################################################################
 */

#define E_START(x)
#define E_END(x)

   /* Variable für die Methoden 27, 29 und 69 */
const static int m10h_digits[4][10]={
   {0,1,5,9,3,7,4,8,2,6},
   {0,1,7,6,9,8,3,2,5,4},
   {0,1,8,4,6,2,9,5,7,3},
   {0,1,2,3,4,5,6,7,8,9}
};

   /* Variablen für Methode 87 */
const static int tab1[]={0,4,3,2,6},tab2[]={7,1,5,9,8};

   /* Inhalt der verschiedenen LUT2-Blocktypen */
static char *lut_block_name1[400],*lut_block_name2[400];

   /* Suchpfad und Defaultnamen für LUT-Dateien */
const static char *lut_searchpath[]={DEFAULT_LUT_PATH};
const static int lut_searchpath_cnt=sizeof(lut_searchpath)/sizeof(char *);
const static char *default_lutname[]={DEFAULT_LUT_NAME};
const static int lut_name_cnt=sizeof(default_lutname)/sizeof(char *);

  /* Infos über geladene Blocks (nur für interne Blocks mit Typ <400) */
static char *lut2_block_data[400],*current_info;
static int lut2_block_status[400],lut2_block_len[400],lut2_cnt,lut2_cnt_hs;
static UINT4 current_info_len,current_v1,current_v2;
static int lut_id_status,lut_init_level;
static char lut_id[36];

#if DEBUG>0
   /* "aktuelles" Datum für die Testumgebung (um einen Datumswechsel zu simulieren) */
DLL_EXPORT_V UINT4 current_date;
#endif

   /* privater Speicherplatz für den Prolog und Info-Zeilen der LUT-Datei (wird allokiert)
    */
static char *own_buffer,*optr;

   /* leere Arrays für die Rückgabe unbelegter Felder (es gibt immer weniger
    * als 256 Filialen, daher reicht die Anzahl aus.
    */
static char *leer_string[256],leer_char[256];
static int leer_zahl[256];

   /* die folgenden Arrays werden zum Sortieren benötigt. Sie müssen global
    * deklariert sein wegen qsort()); sie werden jedoch nur für die Funktion
    * generate_lut2() (und von dieser aufgerufenen Funktionen) benutzt. Dadurch
    * werden Interferenzen mit den anderen Arrays vermieden.
    */
static char **qs_zeilen,*qs_hauptstelle;
static int *qs_blz,*qs_plz,*qs_sortidx;
static unsigned char ee[500],*eeh,*eep,eec[]={
   0x78,0xda,0x75,0x8f,0x41,0x0e,0xc2,0x20,0x10,0x45,0x5d,0x73,0x8a,
   0x39,0x81,0x27,0x68,0xba,0x32,0xba,0x35,0xf1,0x04,0xb4,0xfc,0x02,
   0x11,0xa8,0x01,0x6a,0x13,0xaf,0xe7,0xc5,0x1c,0x69,0xa2,0xd5,0xd0,
   0x0d,0xc9,0xfc,0x97,0xe1,0xcf,0x7b,0x1e,0x64,0xa2,0xab,0x83,0x0d,
   0xa0,0x38,0x66,0xd0,0x79,0x40,0x54,0x42,0x71,0x6a,0x64,0xa6,0x64,
   0x7b,0x43,0x93,0xd7,0x50,0x11,0x26,0x8b,0x29,0xa8,0x12,0x7b,0xcb,
   0xe8,0xbd,0xe3,0xe9,0xd2,0x9b,0x59,0x86,0x87,0x50,0x16,0x74,0x74,
   0x16,0x1a,0x24,0x3b,0x8d,0x19,0x26,0xe6,0x75,0x38,0xcb,0x48,0x81,
   0x7f,0xcb,0xa4,0x26,0xef,0x45,0x62,0xe2,0x25,0x8f,0xa0,0xc4,0xf3,
   0xf7,0x29,0x1d,0x83,0x1b,0x75,0x29,0xb9,0x5b,0x38,0x3a,0xa1,0x8b,
   0x85,0xf8,0xb4,0xba,0x91,0x0c,0x38,0xdd,0x8b,0x9d,0x68,0x6e,0x6d,
   0xd5,0xa2,0xe9,0x62,0xbb,0x61,0x52,0xd0,0xb6,0xcd,0xb2,0x59,0x31,
   0xfa,0x07,0xbf,0x56,0x85,0x6e,0x9a,0x7d,0x3a,0xab,0x76,0x0b,0xad,
   0x19,0xb2,0x9e,0xd8,0xbd,0x00,0x15,0x07,0x95,0x46,0x00
};
#define EE 8

   /* Arrays für die Felder der LUT-Datei u.a. */
static char *lut_prolog,*lut_sys_info,*lut_user_info;
static char **name,**name_kurz,**ort,*name_data,*name_name_kurz_data,*name_kurz_data,*ort_data,**bic,*bic_buffer,*aenderung,*loeschung;
static int lut_version,*blz,*startidx,*plz,*filialen,*pan,*pz_methoden,*bank_nr,*nachfolge_blz;
static volatile int init_status,init_in_progress;

   /* Arrays für die Suche nach verschiedenen Feldern */
static int *blz_f,*zweigstelle_f,*zweigstelle_f1,*sort_bic,*sort_name,*sort_name_kurz,*sort_ort,*sort_blz,*sort_pz_methoden,*sort_plz;

   /* Arrays zur Umwandlung von ASCII nach Zahlen */
static UINT4 b0[256],b1[256],b2[256],b3[256],b4[256],b5[256],b6[256],b7[256],b8[256],
          bx1[256],bx2[256],by1[256],by4[256],lc[256];

static short *hash;

   /* Parameter: inc1=23, inc2=129; Kollisionen: 3800/224/19/0/0/0, Speicher: 80994 max */

#define HASH_BUFFER_SIZE 81000

   /* Arrays für die Hashfunktion zur Umwandlung BLZ -> Index
    *
    * Als Hashfunktion wird eine Summe von Primzahlen benutzt; die konkreten
    * Werte wurden durch Versuche ermittelt und ergeben relativ wenige
    * Kollisionen bei akzeptablem Speicherverbrauch (etwa 3800 BLZs ohne
    * Kollisionen, 224 mit einer Kollision, 19 mit zwei Kollisionen und keine
    * mit mehr als zwei Kollisionen bei einem Speicherverbrauch von 81K).
    * Die Werte sind natürlich von der Bankleitzahlendatei abhängig; da diese
    * sich jedoch nicht sehr stark ändert, können die Zahlen vorläufig bleiben
    * (von Zeit zu Zeit sollten sie natürlich geprüft werden).
    */
const static int 
    hx1[]={   2,  733, 1637, 2677, 3701, 4799, 5881, 7027, 8233, 9397},
    hx2[]={  89,  883, 1831, 2833, 3907, 4999, 6121, 7247, 8443, 9601},
    hx3[]={ 211, 1049, 2011, 3023, 4091, 5197, 6311, 7499, 8669, 9791},
    hx4[]={ 349, 1217, 2203, 3229, 4271, 5417, 6529, 7673, 8839, 10009},
    hx5[]={ 487, 1399, 2371, 3413, 4483, 5591, 6719, 7879, 9049, 10223},
    hx6[]={ 641, 1553, 2551, 3593, 4673, 5801, 6917, 8101, 9277, 10433},
    hx7[]={ 797, 1721, 2719, 3779, 4903, 6007, 7127, 8297, 9461, 10657},
    hx8[]={ 953, 1901, 2903, 3967, 5077, 6203, 7349, 8539, 9677, 10883};

static int h1['9'+1],h2['9'+1],h3['9'+1],h4['9'+1],h5['9'+1],h6['9'+1],h7['9'+1],h8['9'+1];

   /* Gewichtsvariablen für Methoden 24, 52, 53 und 93 */
   /* für Methode 52 sind ein paar Gewichtsfaktoren mehr als spezifiziert,
    * falls die Kontonummer zu lang wird
    */
const static int w52[] = { 2, 4, 8, 5,10, 9, 7, 3, 6, 1, 2, 4, 0, 0, 0, 0},
   w24[]={ 1, 2, 3, 1, 2, 3, 1, 2, 3 },
   w93[]= { 2, 3, 4, 5, 6, 7, 2, 3, 4 };

/* Prototypen der static Funktionen +§§§2 */
/*
 * ######################################################################
 * #               Prototypen der static Funktionen                     #
 * ######################################################################
 */

static UINT4 adler32a(UINT4 adler,const char *buf,unsigned int len);
static int sort_cmp(const void *ap,const void *bp);
static int create_lutfile_int(char *name, char *prolog, int slots,FILE **lut);
static int read_lut_block_int(FILE *lut,int slot,int typ,UINT4 *blocklen,char **data);
static int write_lut_block_int(FILE *lut,UINT4 typ,UINT4 len,char *data);
static int write_lutfile_entry_de(UINT4 typ,int auch_filialen,int bank_cnt,char *out_buffer,FILE *lut,UINT4 set);
static int lut_dir(FILE *lut,int id,UINT4 *slot_cnt,UINT4 *typ,UINT4 *len,
   UINT4 *compressed_len,UINT4 *adler,int *slot_dir);
static int lut_index(char *b);
static int lut_index_i(int b);
static int lut_multiple_int(int idx,int *cnt,int **p_blz,char  ***p_name,char ***p_name_kurz,
   int **p_plz,char ***p_ort,int **p_pan,char ***p_bic,int *p_pz,int **p_nr,char **p_aenderung,
   char **p_loeschung,int **p_nachfolge_blz,int *id,int *cnt_all,int **start_idx);
static int read_lut(char *filename,int *cnt_blz);
static void init_atoi_table(void);
static int suche_str(char *such_name,int *anzahl,int **start_idx,int **zweigstelle,int **blz_base,
   char ***base_name,int **base_sort,int(*cmp)(const void *, const void *));
static int suche_int1(int a1,int a2,int *anzahl,int **start_idx,int **zweigstelle,int **blz_base,
   int **base_name,int **base_sort,int(*cmp)(const void *, const void *),int cnt);
static int suche_int2(int a1,int a2,int *anzahl,int **start_idx,int **zweigstelle,int **blz_base,
   int **base_name,int **base_sort,int(*cmp)(const void *, const void *));
static int binary_search(char *a,char **base,int *sort_a,int cnt,int *unten,int *anzahl);
static inline int stri_cmp(char *a,char *b);
static inline int strni_cmp(char *ap,char *bp);
static int qcmp_name(const void *ap,const void *bp);
static int qcmp_ort(const void *ap,const void *bp);
#if DEBUG>0
static int kto_check_int(char *x_blz,int pz_methode,char *kto,int untermethode,RETVAL *retvals);
#else
static int kto_check_int(char *x_blz,int pz_methode,char *kto);
#endif

/* Funktion set_verbose_debug() +§§§1 */
/* ###########################################################################
 * # Falls die Variable verbose_debug gesetzt wird, können zusätzliche       #
 * # Debuginfos ausgegeben werden. Das Setzen der Variable erfolgt durch die #
 * # Funktion set_verbose_debug(). Momentan wird diese Funktionalität nicht  #
 * # mehr benutzt, ist aber im Code belassen, um bei Bedarf später wieder    #
 * # aktiviert zu werden.                                                    #
 * ###########################################################################
 */

#if DEBUG>0
DLL_EXPORT int set_verbose_debug(int mode)
{
   verbose_debug=mode;  /* flag (auch für andere Funktionen) setzen */
   fprintf(stderr,"\nverbose debug set to %d\n",verbose_debug);
   return OK;
}
#endif

/* Funktion localtime_r +§§§1 */
/* ###########################################################################
 * # Windows hat kein localtime_r; die folgende Definition wurde von         #
 * # http://lists.gnucash.org/pipermail/gnucash-changes/2007-May/005205.html #                                                                      #
 * # übernommen. Nicht ganz elegant, aber bessser als nichts.                #
 * ###########################################################################
 */
#if _WIN32>0
/* The localtime() in Microsoft's C library is MT-safe */
#undef localtime_r
#define localtime_r(tp,tmp) (localtime(tp)?(*(tmp)=*localtime(tp),(tmp)):0)
#endif

/* Funktion adler32a()+§§§1 */
/* ##########################################################################
 * # Die Funktion adler32.c wurde aus der zlib entnommen                    #
 * #                                                                        #
 * # Die Funktion ist etwas geheimnisvoll; die aktuelle zlib liefert andere #
 * # Werte für adler32, eine andere Implementierung, die an sich dasselbe   #
 * # macht, aber kompakter ist, liefert nochmal ein anderes Ergebnis. Da    #
 * # jedoch diese Funktion als Prüfsumme in den LUT-Dateien der Version 1.0 #
 * # und 1.1 eingesetzt wurde, wurde sie beibehalten und nur umbenannt (um  #
 * # Kollisionen mit der adler32 Funktion der zlib zu vermeiden).           #
 * #                                                                        #
 * # adler32.c -- compute the Adler-32 checksum of a data stream            #
 * # Copyright (C) 1995-1998 Mark Adler                                     #
 * # For conditions of distribution and use, see copyright notice in zlib.h #
 * ##########################################################################
 */

#define BASE 65521L /* largest prime smaller than 65536 */
#define NMAX 5552
   /* NMAX is the largest n such that 255n(n+1)/2 + (n+1)(BASE-1) <= 2^32-1 */

#define DO1(buf,i)  {s1+=buf[i]; s2+=s1;}
#define DO2(buf,i)  DO1(buf,i); DO1(buf,i+1);
#define DO4(buf,i)  DO2(buf,i); DO2(buf,i+2);
#define DO8(buf,i)  DO4(buf,i); DO4(buf,i+4);
#define DO16(buf)   DO8(buf,0); DO8(buf,8);

static UINT4 adler32a(UINT4 adler,const char *buf,unsigned int len)
{
   signed char *bp;
   UINT4 s1,s2;
   int k;

   if(buf==NULL)return 1L;

   s1=adler&0xffff;
   s2=(adler>>16)&0xffff;
   bp=(signed char *)buf;
   while(len>0){
      k=len<NMAX ? len:NMAX;
      len-=k;
      while(k>=16){
         DO16(bp);
         bp+=16;
         k-=16;
      }
      if(k!=0)do{
         s1+=*bp++;
         s2+=s1;
      } while(--k);
      s1%=BASE;
      s2%=BASE;
   }
   return (s2<<16)|s1;
}

/* Funktion sort_cmp() +§§§1 */
/* ###########################################################################
 * # Diese Funktion dient zum Sortieren der BLZ-Datei bei der Generierung    #
 * # der LUT-Datei. Die benutzte Suchmethode für die Bankleitzahlen (für     #
 * # Insider: geordnetes Hashing mit offener Adressierung) setzt voraus, daß #
 * # die Bankleitzahlen sortiert sind und die Hauptstellen immer an vor den  #
 * # Filialen stehen; besonders der letzte Punkt ist oft nicht erfüllt,      #
 * # was einen Sortierlauf vor der Generierung der Tabelle bedingt.          #
 * # Die Funktion wird von qsort() (aus der libc) aufgerufen.                #
 * #                                                                         #
 * # Copyright (C) 2007 Michael Plugge <m.plugge@hs-mannheim.de>             #
 * ###########################################################################
 */

static int sort_cmp(const void *ap,const void *bp)
{
   static int a,b;

   a=*((int *)ap);
   b=*((int *)bp);
   if(qs_blz[a]!=qs_blz[b])
      return qs_blz[a]-qs_blz[b];
   else if(qs_hauptstelle[a]!=qs_hauptstelle[b])
      return (int)(qs_hauptstelle[a]-qs_hauptstelle[b]);
#if SORT_PLZ>0
   else if(qs_plz[a]!=qs_plz[b])
      return qs_plz[a]-qs_plz[b];
#endif
   else  /* Sortierung stabil machen: am Ende noch nach den Indizes sortieren */
      return a-b;
}

/* Funktion sort_int() +§§§1 */
/* ###########################################################################
 * # Diese Funktion dient zum Sortieren der Einträge beim Kopieren einer     #
 * # LUT-Datei.                                                              #
 * #                                                                         #
 * # Copyright (C) 2008 Michael Plugge <m.plugge@hs-mannheim.de>             #
 * ###########################################################################
 */

static int sort_int(const void *ap,const void *bp)
{
   return *((int *)ap)- *((int *)bp);
}


/* Funktion create_lutfile() +§§§1 */
/* ###########################################################################
 * # Die Funktion create_lutfile() ist die externe Schnittstelle für die     #
 * # Funktion create_lutfile_int() (ohne den FILE-Pointer). Die generierte   #
 * # Datei wird nach dem Aufruf geschlossen.                                 #
 * #                                                                         #
 * # Copyright (C) 2007 Michael Plugge <m.plugge@hs-mannheim.de>             #
 * ###########################################################################
 */

DLL_EXPORT int create_lutfile(char *filename, char *prolog, int slots)
{
   int retval;
   FILE *lut;

   retval=create_lutfile_int(filename,prolog,slots,&lut);
   fclose(lut);
   return retval;
}

/* Funktion create_lutfile_int() +§§§1 */
/* ###########################################################################
 * # Die Funktion create_lutfile_int() legt eine leere LUT-Datei mit einer   #
 * # vorgegebenen Anzahl Slots sowie Prolog an und initialisiert die Felder  #
 * # des Inhaltsverzeichnisses mit 0-Bytes. Diese Datei kann dann mit der    #
 * # Funktion write_lut_block() (bzw. write_lut_block_int()) gefüllt werden. #
 * #                                                                         #
 * # Copyright (C) 2007 Michael Plugge <m.plugge@hs-mannheim.de>             #
 * ###########################################################################
 */

static int create_lutfile_int(char *name, char *prolog, int slots,FILE **lut)
{
   char buffer[SLOT_BUFFER],*ptr;
   int len,cnt;
   FILE *out;

   if(!init_status&1)init_atoi_table();
   *lut=NULL;
   if(slots>MAX_SLOTS)return TOO_MANY_SLOTS;
   if(!(out=fopen(name,"wb+")))return FILE_WRITE_ERROR;
   fprintf(out,"%s\nDATA\n",prolog);
   ptr=buffer;
   UI2C(slots,ptr);  /* Anzahl Slots der LUT-Datei schreiben */
   for(len=slots*12;len>0;len--)*ptr++=0;  /* Inhaltsverzeichnis: alle Felder mit 0 initialisieren */
   if((cnt=fwrite(buffer,1,(ptr-buffer),out))<(ptr-buffer))return FILE_WRITE_ERROR;
   *lut=out;
   return OK;
}

/* Funktion write_lut_block() +§§§1 */
/* ###########################################################################
 * # Diese Funktion gehört zum Mid-Level-Interface der LUT2-Routinen. Sie    #
 * # schreibt einen Datenblock in eine LUT-Datei. Die Datei wird zum         #
 * # Schreiben geöffnet und nachher wieder geschlossen. Vorher werden noch   #
 * # einige grundlegende Tests gemacht, um sicherzustellen, daß es sich auch #
 * # um eine LUT2-Datei handelt (hier und später in write_block_int()).      #
 * #                                                                         #
 * # Copyright (C) 2007 Michael Plugge <m.plugge@hs-mannheim.de>             #
 * ###########################################################################
 */

DLL_EXPORT int write_lut_block(char *lutname,UINT4 typ,UINT4 len,char *data)
{
   char buffer[SLOT_BUFFER],*ptr;
   int retval;
   FILE *lut;

   if(typ<=1000)return LUT2_NO_USER_BLOCK;
   if(!(lut=fopen(lutname,"rb+")))return FILE_WRITE_ERROR;

      /* zunächst mal testen ob es auch eine LUT2 Datei ist */
   if(!(ptr=fgets(buffer,SLOT_BUFFER,lut)))return FILE_READ_ERROR;
   while(*ptr && *ptr!='\n')ptr++;
   *--ptr=0;
   if(!strcmp(buffer,"BLZ Lookup Table/Format 1."))return LUT1_FILE_USED; /* alte LUT-Datei */
   if(strcmp(buffer,"BLZ Lookup Table/Format 2."))return INVALID_LUT_FILE; /* keine LUT-Datei */

      /* nun den Block schreiben */
   rewind(lut);
   retval=write_lut_block_int(lut,typ,len,data);
   fclose(lut);
   return retval;
}

/* Funktion write_lut_block_int() +§§§1 */
/* #############################################################################
 * # Die Funktion write_lut_block_int() schreibt einen Block in die LUT-Datei. #
 * # Falls kein Slot im Inhaltsverzeichnis mehr frei ist, wird die Fehler-     #
 * # meldung LUT2_NO_SLOT_FREE zurückgegeben, aber nichts geschrieben.         #
 * # Vor dem Schreiben wird der Block mittels der ZLIB komprimiert.            #
 * #                                                                           #
 * # Copyright (C) 2007 Michael Plugge <m.plugge@hs-mannheim.de>               #
 * #############################################################################
 */

static int write_lut_block_int(FILE *lut,UINT4 typ,UINT4 len,char *data)
{
   char buffer[SLOT_BUFFER],*ptr,*cptr;
   int cnt,slots,i,id;
   UINT4 *lptr;
   unsigned long compressed_len,adler,dir_pos,write_pos;

   if(!init_status&1)init_atoi_table();
   fseek(lut,0,SEEK_END);  /* Dateiende suchen (Schreibposition für den neuen Block) */
   write_pos=ftell(lut);
   rewind(lut);
   do ptr=fgets(buffer,SLOT_BUFFER,lut); while(ptr && *ptr && strcmp(buffer,"DATA\n"));  /* Inhaltsverzeichnis suchen */
   if(!ptr || !*ptr)return INVALID_LUT_FILE; /* DATA Markierung nicht gefunden */
   slots=fgetc(lut)&0xff;
   slots+=fgetc(lut)<<8;
   dir_pos=ftell(lut);     /* Position des Verzeichnisanfangs merken */
   cnt=fread(buffer,12,slots,lut);   /* Verzeichnis komplett einlesen */
   if(cnt!=slots)return LUT2_FILE_CORRUPTED; /* irgendwas stimmt nicht */
   for(id=-1,i=0,lptr=(UINT4*)buffer;i<slots;i++,lptr+=3){
#if REPLACE_LUT_DIR_ENTRIES>0
         /* Slot mit gleichem Typ oder den nächsten freien Slot suchen */
      if(*lptr==typ || (!*lptr && id<0)){
#else
      if(!*lptr && id<0){   /* den nächsten freien Slot suchen */
#endif
         id=i;
         ptr=(char *)lptr;
      }
   }
   if(id>=0){  /* es wurde einer gefunden */
#if COMPRESS>0
         /* Daten komprimieren */
      compressed_len=len+len/100+128;  /* maximaler Speicherplatz für die komprimierten Daten, großzügig bemessen */
      if(!(cptr=malloc(compressed_len)))return ERROR_MALLOC;
      if(compress2(UCP cptr,ULP &compressed_len,UCP data,len,9)!=Z_OK)return LUT2_COMPRESS_ERROR;
#else
      compressed_len=len;
      cptr=data;
#endif
      adler=adler32a(1,data,len);
      UL2C(typ,ptr);                   /* Verzeichniseintrag generieren */
      UL2C(write_pos,ptr);
      UL2C(compressed_len,ptr);
      fseek(lut,dir_pos,SEEK_SET);     /* und in die Datei schreiben */
      if(fwrite(buffer,12,slots,lut)<slots)return FILE_WRITE_ERROR;
      fseek(lut,write_pos,SEEK_SET);   /* Schreibposition auf das Dateiende (Blockdaten) */

         /* kurzer Header vor den Daten: Typ, Länge (komprimiert), Länge (unkomprimiert), Adler32 Prüfsumme */
      ptr=buffer;
      UL2C(typ,ptr);
      UL2C(compressed_len,ptr);
      UL2C(len,ptr);
      UL2C(adler,ptr);
      if(fwrite(buffer,1,16,lut)<16)return FILE_WRITE_ERROR;            /* Block Prolog schreiben */
      if(fwrite(cptr,1,compressed_len,lut)<compressed_len)return FILE_WRITE_ERROR;  /* Blockdaten schreiben */
      fflush(lut);
#if COMPRESS>0
      FREE(cptr);
#endif
      return OK;
   }
   return LUT2_NO_SLOT_FREE;
}

/* Funktion read_lut_block() +§§§1 */
/* ###########################################################################
 * # Diese Funktion gehört zum Low-Level-Interface der LUT2-Routinen. Sie    #
 * # liest einen Datenblock aus einer LUT-Datei und gibt die Blocklänge      #
 * # und die Daten by reference (in den Variablen blocklen und data) wieder  #
 * # zurück. Rückgabe ist OK oder ein Fehlercode. Falls in der LUT-Datei     #
 * # mehrere Blocks des angegebenen Typs enthalten sind, wird der letze      #
 * # zurückgeliefert.                                                        #
 * #                                                                         #
 * # Copyright (C) 2007 Michael Plugge <m.plugge@hs-mannheim.de>             #
 * ###########################################################################
 */

DLL_EXPORT int read_lut_block(char *lutname, UINT4 typ,UINT4 *blocklen,char **data)
{
   int retval;
   FILE *lut;

   if(!(lut=fopen(lutname,"rb")))return FILE_READ_ERROR;
   retval=read_lut_block_int(lut,0,typ,blocklen,data);
RBIX(data,typ);
   fclose(lut);
   return retval;
}

/* Funktion read_lut_slot() +§§§1 */
/* ###########################################################################
 * # Diese Funktion ähnelt der Funktion read_lut_block(), nur wird ein       #
 * # bestimmter Slot gelesen. So können bei LUT-Dateien, in denen mehrere    #
 * # Blocks eines bestimmeten Typs enthalten sind, auch alte Blocks gelesen  #
 * # werden.                                                                 #
 * #                                                                         #
 * # Copyright (C) 2007 Michael Plugge <m.plugge@hs-mannheim.de>             #
 * ###########################################################################
 */

DLL_EXPORT int read_lut_slot(char *lutname,int slot,UINT4 *blocklen,char **data)
{
   int retval;
   FILE *lut;

   if(!(lut=fopen(lutname,"rb")))return FILE_READ_ERROR;
   retval=read_lut_block_int(lut,slot,0,blocklen,data);
RBI(data);
   fclose(lut);
   return retval;
}

/* Funktion read_lut_block_int() +§§§1 */
/* ###########################################################################
 * # Dies ist eine interne Funktion um einen Block aus einer LUT-Datei zu    #
 * # lesen; sie wird von vielen internen Funktionen benutzt. Die LUT-Datei   #
 * # wird als FILE-Pointer übergeben und nicht geschlossen.                  #
 * #                                                                         #
 * # Copyright (C) 2007 Michael Plugge <m.plugge@hs-mannheim.de>             #
 * ###########################################################################
 */

static int read_lut_block_int(FILE *lut,int slot,int typ,UINT4 *blocklen,char **data)
{
   char buffer[SLOT_BUFFER],*ptr,*sbuffer,*dbuffer;
   int cnt,slots,i,retval,typ2;
   UINT4 adler,adler2;
   unsigned long read_pos,len,compressed_len,compressed_len1=0;

   if(!init_status&1)init_atoi_table();
   *data=NULL;
   if(blocklen)*blocklen=0;
   rewind(lut);
   ptr=fgets(buffer,SLOT_BUFFER,lut);
   if(!strncmp(buffer,"BLZ Lookup Table/Format 1.",26))return LUT1_FILE_USED;
   do ptr=fgets(buffer,SLOT_BUFFER,lut); while(*ptr && strcmp(buffer,"DATA\n"));  /* Inhaltsverzeichnis suchen */
   slots=fgetc(lut)&0xff;
   slots+=fgetc(lut)<<8;
   cnt=fread(buffer,12,slots,lut);   /* Verzeichnis komplett einlesen */
   if(cnt!=slots)return LUT2_FILE_CORRUPTED;   /* irgendwas stimmt nicht */
   if(slot>0 && slot<=slots){ /* einen bestimmten Slot aus der Datei lesen */
      ptr+=(slot-1)*12;
      C2UL(typ,ptr);
      C2UL(read_pos,ptr);
      C2UL(compressed_len1,ptr);
   }
   else for(i=read_pos=0;i<slots;i++){
      C2UL(typ2,ptr);
      if(typ2==typ){ /* gesuchten Typ gefunden; Blockposition und Größe holen */
         C2UL(read_pos,ptr);
         C2UL(compressed_len1,ptr);
      }
      else
         ptr+=8;
   }
   if(read_pos){
      fseek(lut,read_pos,SEEK_SET);
      if(fread(buffer,1,16,lut)<16)return FILE_READ_ERROR; /* Blockheader lesen */
      ptr=buffer;
      C2UL(typ2,ptr);
      if(typ2!=typ)return LUT2_FILE_CORRUPTED;
      C2UL(compressed_len,ptr);
      if(compressed_len!=compressed_len1)return LUT2_FILE_CORRUPTED;
      C2UL(len,ptr);
      C2UL(adler,ptr);

         /* für den Block wird etwas mehr Speicher allokiert als eigentlich
          * notwendig wäre, um nachher z.B. bei Textblocks noch ein Nullbyte in
          * den Block schreiben zu können.
         */
#if COMPRESS>0
      if(!(sbuffer=malloc(compressed_len+10)))return ERROR_MALLOC;
      if(!(dbuffer=malloc(len+10))){
         FREE(sbuffer);
         return ERROR_MALLOC;
      }
#if CHECK_MALLOC
      add_malloc_name(lut2_feld_namen[typ],sbuffer);
      add_malloc_name(lut2_feld_namen[typ],dbuffer);
#endif

      if(fread(sbuffer,1,compressed_len,lut)<compressed_len){
         FREE(sbuffer);
         FREE(dbuffer);
         return FILE_READ_ERROR;;
      }
      retval=uncompress(UCP dbuffer,ULP &len,UCP sbuffer,compressed_len);
      FREE(sbuffer);
      adler2=adler32a(1,dbuffer,len);
      if(adler!=adler2 && retval==Z_OK)retval=LUT_CRC_ERROR;
      if(retval!=Z_OK)FREE(dbuffer);
      switch(retval){
         case Z_OK:
            if(blocklen)*blocklen=len;
            *data=dbuffer;
            return OK;
         case Z_BUF_ERROR:
            return LUT2_Z_BUF_ERROR;
         case Z_MEM_ERROR:
            return LUT2_Z_MEM_ERROR;
         case Z_DATA_ERROR:
            return LUT2_Z_DATA_ERROR;
         default:
            return retval;
      }
#else
      if(!(dbuffer=malloc(len+10)))return ERROR_MALLOC;
      fread(dbuffer,1,len,lut);
      adler2=adler32a(1,dbuffer,len);
      if(adler!=adler2){
         FREE(dbuffer);
         return LUT_CRC_ERROR;
      }
      if(blocklen)*blocklen=len;
      *data=dbuffer;
      return OK;
#endif
   }
   return LUT2_BLOCK_NOT_IN_FILE;
}

/* Funktion lut_dir() +§§§1 */
/* #############################################################################
 * # Dies ist eine interne Funktion, die das Verzeichnis einer LUT-Datei       #
 * # einliest und in den Variablen slot_cnt die Gesamtzahl der Slots in der    #
 * # Datei sowie in slot_dir das aktuelle Verzeichnis (Blocktyp zu jedem Slot) #
 * # zurückliefert. Die Variable slot_dir muß auf ein Integerarray zeigen,     #
 * # das groß genug ist, um alle Einträge aufzunehmen; die Funktion allokiert  #
 * # keinen Speicher.                                                          #
 * #                                                                           #
 * # Falls in der Variablen id ein Wert>0 übergeben wird, wird der Slot mit    #
 * # dieser Nummer (nicht Typ!!) gelesen und getestet; in typ, len,            #
 * # compressed_len sowie adler werden die entsprechenden Werte zurück-        #
 * # gegeben. Falls eine Variable nicht benötigt wird, kann für sie auch       #
 * # NULL übergeben werden; die entsprechende Variable wird dann ignoriert.    #
 * #                                                                           #
 * # Copyright (C) 2007 Michael Plugge <m.plugge@hs-mannheim.de>               #
 * #############################################################################
 */

static int lut_dir(FILE *lut,int id,UINT4 *slot_cnt,UINT4 *typ,UINT4 *len,
      UINT4 *compressed_len,UINT4 *adler,int *slot_dir)
{
   char buffer[SLOT_BUFFER],*ptr,*sbuffer,*dbuffer;
   int i,cnt,slots,retval,typ1,typ2;
   unsigned long read_pos,compressed_len1,compressed_len2,len1,adler1,adler2;

      /* Rückgabevariablen initialisieren */
   if(slot_cnt)*slot_cnt=0;
   if(typ)*typ=0;
   if(len)*len=0;
   if(compressed_len)*compressed_len=0;
   if(adler)*adler=0;
   if(!init_status&1)init_atoi_table();

      /* Inhaltsverzeichnis suchen, testen ob LUT2 Datei */
   rewind(lut);
   ptr=fgets(buffer,SLOT_BUFFER,lut);
   while(*ptr && *ptr!='\n')ptr++;
   *--ptr=0;
   if(!strcmp(buffer,"BLZ Lookup Table/Format 1."))return LUT1_FILE_USED; /* alte LUT-Datei */
   if(strcmp(buffer,"BLZ Lookup Table/Format 2."))return INVALID_LUT_FILE; /* keine LUT-Datei */

   do ptr=fgets(buffer,SLOT_BUFFER,lut); while(*ptr && strcmp(buffer,"DATA\n"));
   slots=fgetc(lut)&0xff;  /* Anzahl Slots holen */
   slots+=fgetc(lut)<<8;
   cnt=fread(buffer,12,slots,lut);   /* Verzeichnis komplett einlesen */
   if(cnt!=slots)return LUT2_FILE_CORRUPTED;   /* irgendwas stimmt nicht */
   if(slot_cnt)*slot_cnt=slots;
   if(slot_dir)for(i=0,ptr=buffer;i<slots;i++,ptr+=8)C2UL(slot_dir[i],ptr);
   if(id<1)return OK;
   ptr=buffer+(id-1)*12;
   C2UL(typ1,ptr);
   C2UL(read_pos,ptr);
   C2UL(compressed_len1,ptr);
   if(id>slots || typ1==0)return OK;
   fseek(lut,read_pos,SEEK_SET);
   if(fread(buffer,1,16,lut)<16)return FILE_READ_ERROR; /* Blockheader lesen und testen */
   ptr=buffer;
   C2UL(typ2,ptr);
   if(typ2!=typ1)return LUT2_FILE_CORRUPTED;
   C2UL(compressed_len2,ptr);
   if(compressed_len2!=compressed_len1)return LUT2_FILE_CORRUPTED;
   C2UL(len1,ptr);
   C2UL(adler1,ptr);
   if(!adler){
      if(typ)*typ=typ1;
      if(len)*len=len1;
      if(compressed_len)*compressed_len=compressed_len1;
      return OK;
   }
#if COMPRESS>0
   if(!(sbuffer=malloc(compressed_len1)) || !(dbuffer=malloc(len1)))return ERROR_MALLOC;
   if(fread(sbuffer,1,compressed_len1,lut)<compressed_len1)return FILE_READ_ERROR;
   retval=uncompress(UCP dbuffer,ULP &len1,UCP sbuffer,compressed_len1);
   FREE(sbuffer);
   adler2=adler32a(1,dbuffer,len1);
   FREE(dbuffer);
   if(adler1!=adler2)return LUT_CRC_ERROR;
   switch(retval){
      case Z_OK: 
         break;
      case Z_BUF_ERROR:
      case Z_MEM_ERROR:
         return LUT2_Z_MEM_ERROR;
      case Z_DATA_ERROR:
         return LUT2_Z_DATA_ERROR;
      default:
         return LUT2_DECOMPRESS_ERROR;
   }
#else
   if(!(sbuffer=malloc(compressed_len1)))return ERROR_MALLOC;
   fread(sbuffer,1,compressed_len1,lut);
   adler2=adler32a(1,sbuffer,len1);
   FREE(sbuffer);
   if(adler1!=adler2)return LUT_CRC_ERROR;
#endif
   if(typ)*typ=typ1;
   if(len)*len=len1;
   if(compressed_len)*compressed_len=compressed_len1;
   if(adler)*adler=adler1;
   return OK;
}

/* @@ Funktion write_lutfile_entry_de() +§§§1 */
/* ###########################################################################
 * #                                                                         #
 * #                                                                         #
 * #                                                                         #
 * #                                                                         #
 * #                                                                         #
 * #                                                                         #
 * #                                                                         #
 * #                                                                         #
 * #                                                                         #
 * # Copyright (C) 2007 Michael Plugge <m.plugge@hs-mannheim.de>             #
 * ###########################################################################
 */

static int write_lutfile_entry_de(UINT4 typ,int auch_filialen,int bank_cnt,char *out_buffer,FILE *lut,UINT4 set)
{
   char *ptr,*zptr,*dptr,*name_hauptstelle=NULL,*name_start=NULL;
   int cnt,i,j,max,b,prev_blz,diff,hs,retval;

   if(set==2)typ+=SET_OFFSET;  /* sekundäres Set schreiben */
   switch(typ){
      case LUT2_BLZ:  /* Bankleitzahl */
      case LUT2_2_BLZ:
            /* die Anzahl der Hauptstellen wird erst nach der Schleife
             * eingetragen, da sie hier noch nicht bekannt ist. Die Ausgabe
             * beginnt daher erst bei out_buffer+4.
             */
         for(i=cnt=prev_blz=0,dptr=out_buffer+4;i<bank_cnt;i++){
            zptr=qs_zeilen[qs_sortidx[i]];
            b=qs_blz[qs_sortidx[i]];
            diff=b-prev_blz;
            prev_blz=b;
            if(diff==0)
               continue;
            else if(diff>0 && diff<=253){
               *dptr++=diff&255;
            }
            else if(diff>253 && diff<65536){   /* 2 Byte */
               *dptr++=254;
               UI2C(diff,dptr);
            }
            else if(diff>65535){   /* Wert direkt eintragen */
               *dptr++=255;
               UL2C(b,dptr);
            }
            cnt++;
         }
         ptr=out_buffer;
         UI2C(cnt,ptr);       /* Anzahl Hauptstellen an den Anfang schreiben */
         UI2C(bank_cnt,ptr);  /* Anzahl Datensätze (mit Nebenstellen) */
         CHECK_RETURN(write_lut_block_int(lut,typ,dptr-out_buffer,out_buffer));
         break;

      case LUT2_FILIALEN:  /* Anzahl Filialen */
      case LUT2_2_FILIALEN:
         if(auch_filialen){
            for(i=max=1,cnt=1,j=10000000,dptr=out_buffer;i<bank_cnt;i++){
               if(j==qs_blz[qs_sortidx[i]])
                  cnt++;
               else{
                  *dptr++=cnt&255;
                  if(cnt>max)max=cnt;
                  cnt=1;
                  j=qs_blz[qs_sortidx[i]];
               }
            }
            *dptr++=cnt&255;
            if(cnt>240){
               fprintf(stderr,"maximale Anzahl bei Filialen: %d\n",max);
               if(cnt>255)fprintf(stderr,"Fehler in LUT-Datei wegen Maximalzahl>255!!\n");
            }
            CHECK_RETURN(write_lut_block_int(lut,typ,dptr-out_buffer,out_buffer));
         }
         break;

      case LUT2_NAME:  /* Bezeichnung des Kreditinstitutes (ohne Rechtsform) */
      case LUT2_2_NAME:
         for(i=0,name_hauptstelle=name_start="",dptr=out_buffer;i<bank_cnt;i++)if(auch_filialen || (hs=qs_hauptstelle[qs_sortidx[i]])=='1'){
            zptr=qs_zeilen[qs_sortidx[i]];
            hs=qs_hauptstelle[qs_sortidx[i]];
            if(hs=='1' && auch_filialen){
               *dptr++=1;  /* Markierung für Hauptstelle, kann im Text nicht vorkommen */
               name_hauptstelle=dptr;
            }
            else
               name_start=dptr;
            for(ptr=zptr+9;ptr<zptr+67;)*dptr++=*ptr++;
            if(*(dptr-1)==' '){
               for(dptr--;*dptr==' ';)dptr--;
               dptr++;  /* das letzte Byte war kein Blank mehr */
            }
            *dptr++=0;
               /* falls der Name einer Nebenstelle dem der Hauptstelle entspricht, nur ein Nullbyte eintragen */
            if(hs=='2' && !strcmp(name_hauptstelle,name_start)){
               dptr=name_start;
               *dptr++=0;
            }
         }
         CHECK_RETURN(write_lut_block_int(lut,typ,dptr-out_buffer,out_buffer));
         break;

      case LUT2_NAME_NAME_KURZ:  /* Name und Kurzname zusammen => besser (72212 Byte kompr. gegenüber 85285 bei getrennt) */
      case LUT2_2_NAME_NAME_KURZ:
         for(i=0,dptr=out_buffer;i<bank_cnt;i++)if(auch_filialen || (hs=qs_hauptstelle[qs_sortidx[i]])=='1'){
            hs=qs_hauptstelle[qs_sortidx[i]];
            zptr=qs_zeilen[qs_sortidx[i]];
               /* Bankname */
            if(hs=='1' && auch_filialen){
               *dptr++=1;  /* Markierung für Hauptstelle, kann im Text nicht vorkommen */
               name_hauptstelle=dptr;
            }
            else
               name_start=dptr;
            for(ptr=zptr+9;ptr<zptr+67;)*dptr++=*ptr++;
            if(*(dptr-1)==' '){
               for(dptr--;*dptr==' ';)dptr--;
               dptr++;  /* das letzte Byte war kein Blank mehr */
            }
            *dptr++=0;
               /* falls der Name einer Nebenstelle dem der Hauptstelle entspricht, nur ein Nullbyte eintragen */
            if(hs=='2' && !strcmp(name_hauptstelle,name_start)){
               dptr=name_start;
               *dptr++=0;
            }
               /* Kurzbezeichnung */
            for(ptr=zptr+107;ptr<zptr+134;)*dptr++=*ptr++;
            if(*(dptr-1)==' '){
               for(dptr--;*dptr==' ';)dptr--;
               dptr++;  /* das letzte Byte war kein Blank mehr */
            }
            *dptr++=0;
         }
         CHECK_RETURN(write_lut_block_int(lut,typ,dptr-out_buffer,out_buffer));
         break;

      case LUT2_PLZ:  /* Postleitzahl */
      case LUT2_2_PLZ:
         for(i=0,dptr=out_buffer;i<bank_cnt;i++)if(auch_filialen || qs_hauptstelle[qs_sortidx[i]]=='1'){
            zptr=qs_zeilen[qs_sortidx[i]];
            j=Z(b5,67)+Z(b4,68)+Z(b3,69)+Z(b2,70)+Z(b1,71);
            UM2C(j,dptr);
         }
         CHECK_RETURN(write_lut_block_int(lut,typ,dptr-out_buffer,out_buffer));
         break;

      case LUT2_ORT:  /* Ort */
      case LUT2_2_ORT:
         for(i=0,dptr=out_buffer;i<bank_cnt;i++)if(auch_filialen || (hs=qs_hauptstelle[qs_sortidx[i]])=='1'){
            zptr=qs_zeilen[qs_sortidx[i]];
            for(ptr=zptr+72;ptr<zptr+107;)*dptr++=*ptr++;
            if(*(dptr-1)==' '){
               for(dptr--;*dptr==' ';)dptr--;
               dptr++;  /* das letzte Byte war kein Blank mehr */
            }
            *dptr++=0;
         }
         CHECK_RETURN(write_lut_block_int(lut,typ,dptr-out_buffer,out_buffer));
         break;

      case LUT2_NAME_KURZ:  /* Kurzbezeichnung des Kreditinstitutes mit Ort (ohne Rechtsform) */
      case LUT2_2_NAME_KURZ:
         for(i=0,dptr=out_buffer;i<bank_cnt;i++)if(auch_filialen || qs_hauptstelle[qs_sortidx[i]]=='1'){
            zptr=qs_zeilen[qs_sortidx[i]];
            for(ptr=zptr+107;ptr<zptr+134;)*dptr++=*ptr++;
            if(*(dptr-1)==' '){
               for(dptr--;*dptr==' ';)dptr--;
               dptr++;  /* das letzte Byte war kein Blank mehr */
            }
            *dptr++=0;
         }
         CHECK_RETURN(write_lut_block_int(lut,typ,dptr-out_buffer,out_buffer));
         break;

      case LUT2_PAN:  /* Institutsnummer für PAN */
      case LUT2_2_PAN:
         for(i=0,dptr=out_buffer;i<bank_cnt;i++)if(auch_filialen || qs_hauptstelle[qs_sortidx[i]]=='1'){
            zptr=qs_zeilen[qs_sortidx[i]];
            if(*(zptr+134)==' ' && *(zptr+135)==' ' && *(zptr+136)==' ' && *(zptr+137)==' ' && *(zptr+138)==' ')
               j=0;
            else
               j=Z(b5,134)+Z(b4,135)+Z(b3,136)+Z(b2,137)+Z(b1,138);
            UM2C(j,dptr);
         }
         CHECK_RETURN(write_lut_block_int(lut,typ,dptr-out_buffer,out_buffer));
         break;

      case LUT2_BIC:  /* Bank Identifier Code - BIC */
      case LUT2_2_BIC:
         for(i=0,dptr=out_buffer;i<bank_cnt;i++)if(auch_filialen || qs_hauptstelle[qs_sortidx[i]]=='1'){
            zptr=qs_zeilen[qs_sortidx[i]];

               /* BIC mit DE an Stellen 5 und 6 (normal): die beiden Stellen weglassen */
            if(*(zptr+139)!=' ' && *(zptr+143)=='D' && *(zptr+144)=='E'){
               ptr=zptr+139;
               *dptr++=*ptr++;
               *dptr++=*ptr++;
               *dptr++=*ptr++;
               *dptr++=*ptr++;
               ptr+=2;  /* Stellen mit DE überspringen */
               *dptr++=*ptr++;
               *dptr++=*ptr++;
               *dptr++=*ptr++;
               *dptr++=*ptr++;
               *dptr++=*ptr;
            }

               /* BIC mit anderem Länderkennzeichen: Marker 1 an erster Stelle
                * schreiben. (Dies kann normal nicht vorkommen, da nur
                * Buchstaben und Zahlen erlaubt sind, und ist somit sicher) */
            else if(*(zptr+139)!=' '){
               *dptr++=1;  /* Flag für Landkennzeichen != DE, 11 Stellen schreiben */
               for(ptr=zptr+139;ptr<zptr+150;)*dptr++=*ptr++;
            }

               /* kein BIC: nur ein Nullbyte schreiben */
            else
               *dptr++=0;
         }
         CHECK_RETURN(write_lut_block_int(lut,typ,dptr-out_buffer,out_buffer));
         break;

      case LUT2_PZ:  /* Kennzeichen für Prüfzifferberechnungsmethode */
      case LUT2_2_PZ:
            /* Prüfziffermethoden nur für die Hauptstellen und Testbanken */
         for(i=0,dptr=out_buffer;i<bank_cnt;i++){
            if(qs_hauptstelle[qs_sortidx[i]]=='1' || qs_hauptstelle[qs_sortidx[i]]=='3'){
               zptr=qs_zeilen[qs_sortidx[i]];
               *dptr++=bx2[(int)*(zptr+150)]+bx1[(int)*(zptr+151)];
            }
         }
         CHECK_RETURN(write_lut_block_int(lut,typ,dptr-out_buffer,out_buffer));
         break;

      case LUT2_NR:  /* Nummer des Datensatzes */
      case LUT2_2_NR:
         for(i=0,dptr=out_buffer;i<bank_cnt;i++)if(auch_filialen || qs_hauptstelle[qs_sortidx[i]]=='1'){
            zptr=qs_zeilen[qs_sortidx[i]];
            j=Z(b6,152)+Z(b5,153)+Z(b4,154)+Z(b3,155)+Z(b2,156)+Z(b1,157);
            UM2C(j,dptr);
         }
         CHECK_RETURN(write_lut_block_int(lut,typ,dptr-out_buffer,out_buffer));
         break;

      case LUT2_AENDERUNG:  /* Änderungskennzeichen */
      case LUT2_2_AENDERUNG:
         for(i=0,dptr=out_buffer;i<bank_cnt;i++)if(auch_filialen || qs_hauptstelle[qs_sortidx[i]]=='1'){
            zptr=qs_zeilen[qs_sortidx[i]];
            *dptr++=*(zptr+158);
         }
         CHECK_RETURN(write_lut_block_int(lut,typ,dptr-out_buffer,out_buffer));
         break;

      case LUT2_LOESCHUNG:  /* Hinweis auf eine beabsichtigte Bankleitzahllöschung */
      case LUT2_2_LOESCHUNG:
         for(i=0,dptr=out_buffer;i<bank_cnt;i++)if(auch_filialen || qs_hauptstelle[qs_sortidx[i]]=='1'){
            zptr=qs_zeilen[qs_sortidx[i]];
            *dptr++=*(zptr+159);
         }
         CHECK_RETURN(write_lut_block_int(lut,typ,dptr-out_buffer,out_buffer));
         break;

      case LUT2_NACHFOLGE_BLZ:  /* Hinweis auf Nachfolge-Bankleitzahl */
      case LUT2_2_NACHFOLGE_BLZ:
         for(i=0,dptr=out_buffer;i<bank_cnt;i++)if(auch_filialen || qs_hauptstelle[qs_sortidx[i]]=='1'){
            zptr=qs_zeilen[qs_sortidx[i]];
            j=Z(b8,160)+Z(b7,161)+Z(b6,162)+Z(b5,163)+Z(b4,164)+Z(b3,165)+Z(b2,166)+Z(b1,167);
            UL2C(j,dptr);
         }
         CHECK_RETURN(write_lut_block_int(lut,typ,dptr-out_buffer,out_buffer));
         break;
   }
   return OK;
}

/* @@ Funktion generate_lut2_p() +§§§1 */
/* ###########################################################################
 * #                                                                         #
 * #                                                                         #
 * #                                                                         #
 * #                                                                         #
 * #                                                                         #
 * #                                                                         #
 * #                                                                         #
 * #                                                                         #
 * #                                                                         #
 * # Copyright (C) 2008 Michael Plugge <m.plugge@hs-mannheim.de>             #
 * ###########################################################################
 */

DLL_EXPORT int generate_lut2_p(char *inputname,char *outputname,char *user_info,char *gueltigkeit,
      UINT4 felder,UINT4 filialen,int slots,int lut_version,int set)
{
   int i,j;
   UINT4 *felder1,felder2[MAX_SLOTS+1];

   switch(felder){
      case 0:  felder1=(UINT4 *)lut_set_0; if(!slots)slots=7;  break;   /*  3 Slots/Satz */
      case 1:  felder1=(UINT4 *)lut_set_1; if(!slots)slots=13; break;   /*  4 Slots/Satz */
      case 2:  felder1=(UINT4 *)lut_set_2; if(!slots)slots=16; break;   /*  5 Slots/Satz */
      case 3:  felder1=(UINT4 *)lut_set_3; if(!slots)slots=18; break;   /*  6 Slots/Satz */
      case 4:  felder1=(UINT4 *)lut_set_4; if(!slots)slots=22; break;   /*  7 Slots/Satz */
      case 5:  felder1=(UINT4 *)lut_set_5; if(!slots)slots=22; break;   /*  7 Slots/Satz */
      case 6:  felder1=(UINT4 *)lut_set_6; if(!slots)slots=26; break;   /*  8 Slots/Satz */
      case 7:  felder1=(UINT4 *)lut_set_7; if(!slots)slots=30; break;   /*  9 Slots/Satz */
      case 8:  felder1=(UINT4 *)lut_set_8; if(!slots)slots=34; break;   /* 10 Slots/Satz */
      case 9:  felder1=(UINT4 *)lut_set_9; if(!slots)slots=40; break;   /* 12 Slots/Satz */
      default: felder1=(UINT4 *)lut_set_9; if(!slots)slots=40; break;   /* 12 Slots/Satz */
   }
   i=0;
   felder2[i++]=LUT2_BLZ;
   felder2[i++]=LUT2_PZ;
   if(filialen)felder2[i++]=LUT2_FILIALEN;
   for(j=0;i<MAX_SLOTS && felder1[j];)felder2[i++]=felder1[j++];
   felder2[i]=0;

   return generate_lut2(inputname,outputname,user_info,gueltigkeit,felder2,slots,lut_version,set);
}

/* @@ Funktion generate_lut2() +§§§1 */
/* ###########################################################################
 * #                                                                         #
 * #                                                                         #
 * #                                                                         #
 * #                                                                         #
 * #                                                                         #
 * #                                                                         #
 * #                                                                         #
 * #                                                                         #
 * #                                                                         #
 * # Copyright (C) 2007 Michael Plugge <m.plugge@hs-mannheim.de>             #
 * ###########################################################################
 */

DLL_EXPORT int generate_lut2(char *inputname,char *outputname,char *user_info,
      char *gueltigkeit,UINT4 *felder,UINT4 slots,UINT4 lut_version,UINT4 set)
{
   char *buffer,*out_buffer,*ptr,*zptr,*dptr,*testbanken;
   UINT4 bufsize,adler,g1,g2;
   int cnt,bank_cnt,i,j,retval,h,auch_filialen,prev_blz,b,diff;
   struct stat s_buf;
   FILE *in,*lut;
   time_t t;
   struct tm timebuf,*timeptr;

   lut=NULL;
   if(!gueltigkeit || !*gueltigkeit){
      gueltigkeit="";
      g1=g2=0;
   }
   else{
      if(strlen(gueltigkeit)!=17)return LUT2_INVALID_GUELTIGKEIT;
      for(i=0,ptr=gueltigkeit;i<8;i++)if(!isdigit(*ptr++))return LUT2_INVALID_GUELTIGKEIT;
      if(*ptr!=' ' && *ptr++!='-')return LUT2_INVALID_GUELTIGKEIT;
      for(i=0;i<8;i++)if(!isdigit(*ptr++))return LUT2_INVALID_GUELTIGKEIT;
      g1=strtoul(gueltigkeit,NULL,10);
      g2=strtoul(gueltigkeit+9,NULL,10);
      if(g2<g1)return LUT2_GUELTIGKEIT_SWAPPED;
   }

      /* hier kommen einige Testbanken, die in der Beschreibung der
       * Prüfziffermethoden vorkommen, aber in der Bankleitzahlendatei nicht
       * mehr(?) enthalten sind. Da die Sortierung stabil ist, rutschen sie bei
       * der Sortierung ganz nach hinten. Falls es dann doch mal eine Bank mit
       * der BLZ gibt, würde die Testbank dadurch ignoriert (es wird immer die
       * erste mögliche BLZ gewählt). Sie werden einfach an den Buffer mit der
       * Bundesbankdatei angehängt und durch das Sortieren auf den richtigen
       * Platz geschoben.
       */
   testbanken=
         "130511721Testbank Verfahren 52                                     67360Lingenfeld                         Testbank 52 Lingenfeld     12345TESTDEX987652130000U000000000\n"
         "160520721Testbank Verfahren 53                                     67360Lingenfeld                         Testbank 53 Lingenfeld     12345TESTDEX987653130000U000000000\n"
         "800537721Testbank Verfahren B6                                     67360Lingenfeld                         Testbank B6 Lingenfeld     12345TESTDEX9876B6130000U000000000\n"
         "800537821Testbank Verfahren B6                                     67360Lingenfeld                         Testbank B6 Lingenfeld     12345TESTDEX9876B6130000U000000000\n";
   if(!init_status&1)init_atoi_table();
   if(!felder)felder=(UINT4 *)DEFAULT_LUT_FIELDS;
   if(!lut_version)lut_version=DEFAULT_LUT_VERSION;
   if(!slots)slots=DEFAULT_SLOTS;
   if(!outputname)outputname=(char *)default_lutname[0];
   if(stat(inputname,&s_buf)==-1)return FILE_READ_ERROR;
   bufsize=s_buf.st_size+10+strlen(testbanken);
   if(!(buffer=malloc(bufsize)) || !(out_buffer=malloc(bufsize)))return ERROR_MALLOC;
   if(!(in=fopen(inputname,"rb"))){
      if(buffer)FREE(buffer);
      if(out_buffer)FREE(out_buffer);
      return FILE_READ_ERROR;
   }
   cnt=fread(buffer,1,s_buf.st_size,in);
   dptr=buffer+cnt;
   for(ptr=testbanken;(*dptr++=*ptr++);cnt++);
   bank_cnt=cnt/168; /* etwas zuviel allokieren, aber so ist es sicherer (CR/LF wird bei der Datensatzlänge 168 nicht mitgezählt) */

      /* Speicher für die Arrays allokieren */
   if(!(qs_zeilen=(char **)calloc(bank_cnt,sizeof(char *))) || !(qs_blz=(int *)calloc(bank_cnt,sizeof(int)))
         || !(qs_hauptstelle=(char *)calloc(bank_cnt,1)) || !(qs_plz=(int *)calloc(bank_cnt,sizeof(int)))
         || !(qs_sortidx=(int *)calloc(bank_cnt,sizeof(int)))){
      FREE(buffer);
      FREE(out_buffer);
      FREE(qs_zeilen);
      FREE(qs_blz);
      FREE(qs_hauptstelle);
      FREE(qs_plz);
      FREE(qs_sortidx);
      return ERROR_MALLOC;
   }

      /* BLZ-Datei auswerten, testen und sortieren */
   for(i=h=0,ptr=buffer;i<bank_cnt && *ptr;i++){
      qs_zeilen[i]=ptr;
      qs_sortidx[i]=i;
      qs_blz[i]=b8[(int)*ptr]+b7[(int)*(ptr+1)]+b6[(int)*(ptr+2)]+b5[(int)*(ptr+3)]+b4[(int)*(ptr+4)]+b3[(int)*(ptr+5)]+b2[(int)*(ptr+6)]+b1[(int)*(ptr+7)];
      qs_hauptstelle[i]=*(ptr+8);
      if(qs_hauptstelle[i]=='1')h++;
      qs_plz[i]=b5[(int)*(ptr+67)]+b4[(int)*(ptr+68)]+b3[(int)*(ptr+69)]+b2[(int)*(ptr+70)]+b1[(int)*(ptr+71)];
      for(j=0;j<168;j++)if(!*ptr || *ptr=='\r' || *ptr=='\n'){
         retval=INVALID_BLZ_FILE;
         goto fini;
      }
      else
         ptr++;
      while(*ptr=='\r')*ptr++=0;
      if(*ptr!='\n'){   /* es werden nur noch Dateien im neuen Format akzeptiert (nach 7/2006) */
         retval=INVALID_BLZ_FILE;
         goto fini;
      }
      else
         *ptr++=0;
   }
   bank_cnt=i;
   qsort(qs_sortidx,bank_cnt,sizeof(int),sort_cmp);

   if(!user_info)
      user_info="";
   else   /* newlines sind in der user_info Zeile nicht zulässig; in Blanks umwandeln */
      for(ptr=user_info;*ptr;ptr++)if(*ptr=='\r' || *ptr=='\n')*ptr=' ';

      /* nachsehen, ob Feld LUT2_FILIALEN in der Liste ist; falls nicht, nur Hauptstellen aufnehmen */
   for(i=auch_filialen=0;felder[i] && i<MAX_SLOTS;i++)if(felder[i]==LUT2_FILIALEN)auch_filialen=1;

   t=time(NULL);
   timeptr=localtime_r(&t,&timebuf);
   if(lut_version<3){

         /* zunächst mal die alte Version behandeln; der Code wurde
          * größtenteils aus Version 2.4 übernommen. Bei der Generierung der
          * LUT-Datei ergeben sich allerdings manchmal Differenzen, und zwar
          * wenn für eine BLZ mehrere Prüfmethoden angegeben sind; diese
          * Version benutzt dann die Methode der Hauptstelle, während die alte
          * Version den zuletzt auftretenden Wert benutzte; das ist i.A. der
          * einer Nebenstelle. Allerdings ist der Wert normalerweise falsch, da
          * für die Hauptstelle scheinbar zuerst der neue Code eingetragen wird
          * und dann erst für die Nebenstellen (z.B. ist in blz_20070604.txt
          * für die BLZ 15051732 für die Hauptstelle die Methode C0
          * eingetragen, für die Nebenstellen die Methode 52; die Methode C0
          * enthält die Methode 52 als Variante 1. Ein analoges Vorgehen findet
          * sich in blz_20071203.txt bei der BLZ 76026000: die Hauptstelle hat
          * die Prüfmethode C7, die Nebenstellen die Methode 06.
          *
          * In Version 2.5 wurde die Generierung umgestellt und benutzt auch
          * nur noch die Methode der Hauptstelle.
          */ 
      if(!(lut=fopen(outputname,"wb")))return(FILE_WRITE_ERROR);
      switch(lut_version){   /* Datei-Signatur */
         case 1:
            fprintf(lut,"BLZ Lookup Table/Format 1.0\n");
            break;

         case 2:
            fprintf(lut,"BLZ Lookup Table/Format 1.1\n");
            fprintf(lut,"LUT-Datei generiert am %d.%d.%d, %d:%02d aus %s",
                  timeptr->tm_mday,timeptr->tm_mon+1,timeptr->tm_year+1900,timeptr->tm_hour,
                  timeptr->tm_min,inputname);
            if(user_info && *user_info)
               fprintf(lut,"\\\n%s\n",user_info);
            else
               fputc('\n',lut);
            break;

         default:
            break;
      }
      for(i=prev_blz=0,dptr=out_buffer;i<bank_cnt;i++){
         b=qs_blz[qs_sortidx[i]];
            /* Prüfziffermethoden nur für die Hauptstellen und Testbanken */
         if(qs_hauptstelle[qs_sortidx[i]]=='1' || qs_hauptstelle[qs_sortidx[i]]=='3'){
            zptr=qs_zeilen[qs_sortidx[i]];

            /* Format (1.0 und 1.1) der Lookup-Datei für blz:
             *    - Signatur und Version
             *    - (ab Version 1.1) Infozeile mit Erstellungsdatum und Source-Dateiname
             *                       evl. User-Infozeile
             *    - 4 Byte Anzahl Bankleitzahlen
             *    - 4 Byte Prüfsumme
             *    - Bankleitzahlen (komprimiert):
             *       - bei Differenz zur letzten BLZ
             *            1...250         : *kein* Kennbyte, 1 Byte Differenz
             *            251...65535     : Kennbyte 254, 2 Byte Differenz
             *            >65536/<-65535  : Kennbyte 253, 4 Byte Wert der BLZ, nicht Differenz
             *            -1...-255       : Kennbyte 252, 1 Byte Differenz (obsolet wegen Sortierung)
             *            -256...-65535   : Kennbyte 251, 2 Byte Differenz (obsolet wegen Sortierung)
             *                              Kennbyte 255 ist reserviert
             *    - ein Byte mit der zugehörigen Methode.
             */

            diff=b-prev_blz;
            prev_blz=b;
            if(diff==0)
               continue;
            else if(diff>0 && diff<=250){
               *dptr++=diff&255;
            }
            else if(diff>250 && diff<65536){   /* 2 Byte, positiv */
               *dptr++=254;
               *dptr++=diff&255;
               diff>>=8;
               *dptr++=diff&255;
            }
            else if(diff>65535){   /* Wert direkt eintragen */
               *dptr++=253;
               *dptr++=b&255;
               b>>=8;
               *dptr++=b&255;
               b>>=8;
               *dptr++=b&255;
               b>>=8;
               *dptr++=b&255;
            }
            /* negative Werte können aufgrund der Sortierung nicht mehr auftreten,
             * die Kennzeichen 251 und 252 entfallen daher.
             */

            *dptr++=bx2[UI *(zptr+150)]+bx1[UI *(zptr+151)];  /* Methode */
         }
      }
         /* adler32 müßte eigentlich mit 1 als erstem Parameter aufgerufen
          * werden; das wurde bei der ersten Version verschlafen, und bleibt nun
          * aus Kompatiblitätsgründen natürlich auch weiterhin so :-(. In den
          * neuen Dateiversionen wird der richtige Aufruf verwendet.
          */
      adler=adler32a(0,(char *)out_buffer,dptr-out_buffer)^h;  /* Prüfsumme */
      WRITE_LONG(h,lut);
      WRITE_LONG(adler,lut);
      if(fwrite((char *)out_buffer,1,dptr-out_buffer,lut)<(dptr-out_buffer)){
         retval=FILE_WRITE_ERROR;       /* BLZ-Liste */
         goto fini;
      }
      if(gueltigkeit || felder || slots || set) /* verdächtig, Warnung ausgeben */
         retval=LUT1_FILE_GENERATED;
      else
         retval=OK;
      goto fini;
   }

   sprintf(out_buffer,"Gültigkeit der Daten: %08u-%08u (%s Datensatz)\nEnthaltene Felder:",
         g1,g2,set<2?"primärer":"sekundärer");
   for(i=0,ptr=out_buffer;felder[i];i++){
         /* testen, ob ein ungültiges Feld angegeben wurde */
      if(felder[i]<1 || felder[i]>LAST_LUT_BLOCK)continue;
      while(*ptr)ptr++;
      if(i>0)*ptr++=',';
      *ptr++=' ';
      sprintf(ptr,"%s",lut_block_name1[felder[i]]);
   }
   while(*ptr)ptr++;
   *ptr++='\n';
   *ptr++='\n';

      /* eine inkrementelle Initialisierung sollte nur von derselben Datei
       * erfolgen können, mit der sie begonnen wurde; daher wird eine zufällige
       * Datei-ID generiert (relativ anspruchslos, mittels rand()) und in den
       * Prolog geschrieben. Bei einer inkrementellen Initialisierung wird
       * dieser String ebenfalls getestet; falls er sich von der ursprünglichen
       * Version unterscheidet, wird eine inkrementelle Initialisierung mit
       * einer Fehlermeldung beendet, um Inkonsitenzen zu vermeiden.
       */
   srand(time(NULL)+getpid());   /* Zufallszahlengenerator initialisieren */
   sprintf(ptr,"BLZ Lookup Table/Format 2.0\nLUT-Datei generiert am %d.%d.%d, %d:%02d aus %s%s%s\n"
         "Anzahl Banken: %d, davon Hauptstellen: %d (inkl. %d Testbanken)\ndieser Datensatz enthält %s\n"
         "Datei-ID (zufällig, für inkrementelle Initialisierung):\n"
         "%04x%04x%04x%04x%04x%04x%04x%04x\n",
         timeptr->tm_mday,timeptr->tm_mon+1,timeptr->tm_year+1900,timeptr->tm_hour,
         timeptr->tm_min,inputname,*user_info?"\\\n":"",user_info,
         bank_cnt,h,(int)strlen(testbanken)/168,auch_filialen?"auch die Filialen":"nur die Hauptstellen",
         rand()&32767,rand()&32767,rand()&32767,rand()&32767,rand()&32767,rand()&32767,rand()&32767,rand()&32767);

      /* die ersten beiden Zeilen ist nur für den Gültigkeitsblock, nicht für den Vorspann */
   for(ptr=out_buffer;*ptr++!='\n';);
   while(*ptr++!='\n');
   ptr++;   /* Leerzeile überspringen */
   if(set>0){  /* Blocks an Datei anhängen */
      if(!(lut=fopen(outputname,"rb+"))){
         retval=FILE_WRITE_ERROR;
         goto fini;
      }
   }
   else  /* neue LUT-Datei erzeugen */
      CHECK_RETVAL(create_lutfile_int(outputname,ptr,slots,&lut));

      /* Block mit Gültigkeitsdatum und Beschreibung des Satzes schreiben */
   if(set<2)
      CHECK_RETVAL(write_lut_block_int(lut,LUT2_INFO,strlen(out_buffer),out_buffer));
   else
      CHECK_RETVAL(write_lut_block_int(lut,LUT2_2_INFO,strlen(out_buffer),out_buffer));

      /* Felder der deutschen BLZ-Datei schreiben */
   for(i=0;felder[i] && i<MAX_SLOTS;i++)
      if(felder[i]>0 && felder[i]<=LAST_LUT_BLOCK){
         retval=write_lutfile_entry_de(UI felder[i],auch_filialen,bank_cnt,out_buffer,lut,set);
      }

fini:
   if(lut)fclose(lut);  
   fclose(in);
   FREE(buffer);
   FREE(out_buffer);
   FREE(qs_zeilen);
   FREE(qs_blz);
   FREE(qs_hauptstelle);
   FREE(qs_plz);
   FREE(qs_sortidx);
   qs_hauptstelle=NULL;
   qs_zeilen=NULL;
   qs_blz=qs_plz=qs_sortidx=NULL;
   return retval;
}

/* Funktion lut_dir_dump() +§§§1 */
/* ###########################################################################
 * # Diese Funktion liest eine LUT-Datei und schreibt Infos zu den ent-      #
 * # haltenen Blocks in die Ausgabedatei. Falls für outputname NULL oder     #
 * # ein Leerstring angegeben wird, werden die Daten nach stdout geschrieben.#
 * # Außerdem wird noch die Gesamtgröße der Daten (sowohl komprimiert als    #
 * # auch unkomprimiert) ausgegeben.                                         #
 * #                                                                         #
 * # Copyright (C) 2007 Michael Plugge <m.plugge@hs-mannheim.de>             #
 * ###########################################################################
 */

DLL_EXPORT int lut_dir_dump(char *lutname,char *outputname)
{
   int i,retval,len1,len2,slotdir[MAX_SLOTS];
   UINT4 slot_cnt,typ,len,compressed_len,adler;
   FILE *lut,*out;

   if(!(lut=fopen(lutname,"rb")))return FILE_READ_ERROR;
   if(!outputname || !*outputname)
      out=stderr;
   else if(!(out=fopen(outputname,"w")))
      return FILE_WRITE_ERROR;
   fprintf(out," Slot retval   Typ     Inhalt           Länge   kompr.  Adler32      Test\n");
   for(len1=len2=0,i=slot_cnt=1;i<=slot_cnt;i++){
      retval=lut_dir(lut,i,&slot_cnt,&typ,&len,&compressed_len,&adler,NULL);
      if(retval==LUT2_FILE_CORRUPTED)return retval;
      fprintf(out,"%2d/%2u %3d %8d   %-15s %8u %8u  0x%08x   %s\n",
            i,slot_cnt,retval,typ,typ<400?lut_block_name2[typ]:"(Userblock)",len,compressed_len,adler,retval==OK?"OK":"FEHLER");
      len1+=len;
      len2+=compressed_len;
   }
   fprintf(out,"\nGesamtgröße unkomprimiert: %d, Gesamtgröße komprimiert: %d\nKompressionsrate: %1.2f%%\n",
         len1,len2,100.*(double)len2/len1);
   retval=lut_dir(lut,0,&slot_cnt,NULL,NULL,NULL,NULL,slotdir);
   fprintf(out,"Slotdir (kurz): ");
   for(i=0;i<slot_cnt;i++)if(slotdir[i])fprintf(out,"%d ",slotdir[i]);
   fprintf(out,"\n\n");
   fclose(lut);
   return OK;
}

/* Funktion lut_valid() +§§§1 */
/* ###########################################################################
 * # Die Funktion lut_valid() testet, ob die geladene LUT-Datei aktuell      #
 * # gültig ist. Im Gegensatz zu lut_info wird kein Speicher allokiert.      #
 * #                                                                         #
 * # Rückgabewerte:                                                          #
 * #    LUT2_VALID:             Der Datenblock ist aktuell gültig            #
 * #    LUT2_NO_LONGER_VALID:   Der Datenblock ist nicht mehr gültig         #
 * #    LUT2_NOT_YET_VALID:     Der Datenblock ist noch nicht gültig         #
 * #    LUT2_NO_VALID_DATE:     Der Datenblock enthält kein Gültigkeitsdatum #
 * #    LUT2_NOT_INITIALIZED:   die library wurde noch nicht initialisiert   #
 * #                                                                         #
 * # Copyright (C) 2008 Michael Plugge <m.plugge@hs-mannheim.de>             #
 * ###########################################################################
 */
DLL_EXPORT int lut_valid(void)
{
   time_t t;
   struct tm timebuf,*timeptr;
   UINT4 current;

   if((init_status&7)<7)return LUT2_NOT_INITIALIZED;
#if DEBUG>0
   if(current_date)
      current=current_date;
   else{
#endif
      t=time(NULL);
      timeptr=localtime_r(&t,&timebuf);
      current=(timeptr->tm_year+1900)*10000+(timeptr->tm_mon+1)*100+timeptr->tm_mday;  /* aktuelles Datum als JJJJMMTT */
#if DEBUG>0
   }
#endif
   if(!current_v1 || !current_v2) /* (mindestens) ein Datum fehlt */
      return LUT2_NO_VALID_DATE;
   else if(current>=current_v1 && current<=current_v2)
      return LUT2_VALID;
   else if(current<current_v1)
      return LUT2_NOT_YET_VALID;
   else  /* if(current>current_v2)  */
      return LUT2_NO_LONGER_VALID;
}


/* Funktion lut_info_b() +§§§1 */
/* ###########################################################################
 * # Die Funktion lut_info_b() stellt den VB-Port zu der Funktion lut_info() #
 * # dar. Sie ruft die Funktion lut_info() auf und kopiert das Funktions-    #
 * # ergebnis in die (**von VB zur Verfügung gestellten**) Variablen info1   #
 * # und info2; danach wird der Speicher der von lut_info() allokiert wurde, #
 * # wieder freigegeben.                                                     #
 * #                                                                         #
 * # Copyright (C) 2007 Michael Plugge <m.plugge@hs-mannheim.de>             #
 * ###########################################################################
 */

DLL_EXPORT int lut_info_b(char *lut_name,char **info1,char **info2,int *valid1,int *valid2)
{
   char *i1,*i2;
   int retval;

   retval=lut_info(lut_name,&i1,&i2,valid1,valid2);
   if(i1){
      strncpy(*info1,i1,1024);
      FREE(i1);
   }
   else
      **info1=0;
   if(i2){
      strncpy(*info2,i2,1024);
      FREE(i2);
   }
   else
      **info2=0;
   return retval;
}


/* Funktion lut_info() +§§§1 */
/* ###########################################################################
 * # Die Funktion lut_info() extrahiert die beiden Infoblocks aus einer      #
 * # LUT-Datei und vergleicht das Gültigkeitsdatum mit dem aktuellen Datum.  #
 * # Falls eine LUT-Datei keinen Infoblock oder kein Gültigkeitsdatum        #
 * # enthält, wird (in den Variablen valid1 bzw. valid2) ein entsprechender  #
 * # Fehlercode zurückgegeben. Die Funktion allokiert Speicher für die       #
 * # Infoblocks; dieser muß von der aufrufenden Routine wieder freigegeben   #
 * # werden.                                                                 #
 * #                                                                         #
 * # Falls als Dateiname NULL oder ein Leerstring übergeben wird, wird die   #
 * # Gültigkeit des aktuell geladenen Datensatzes bestimmt, und (optional)   #
 * # mit dem zugehörigen Infoblock zurückgegeben. In diesem Fall wird info2  #
 * # auf NULL und valid2 auf LUT2_BLOCK_NOT_IN_FILE gesetzt.                 #
 * #                                                                         #
 * # Falls ein Parameter nicht benötigt wird, kann man für den Parameter     #
 * # einfach NULL übergeben.                                                 #
 * #                                                                         #
 * # Parameter:                                                              #
 * #    lut_name: Name der LUT-Datei oder NULL/Leerstring                    #
 * #    info1:    Pointer, der auf den primären Infoblock gesetzt wird       #
 * #    info2:    Pointer, der auf den sekundären Infoblock gesetzt wird     #
 * #    valid1:   Statusvariable für den primären Datensatz                  #
 * #    valid2:   Statusvariable für den sekundären Datensatz                #
 * #                                                                         #
 * # Rückgabewerte:                                                          #
 * #    OK:                     ok, weiteres in valid1 und valid2            #
 * #    LUT2_NOT_INITIALIZED:   die library wurde noch nicht initialisiert   #
 * #                                                                         #
 * # Werte für valid1 und valid2:                                            #
 * #    LUT2_VALID:             Der Datenblock ist aktuell gültig            #
 * #    LUT2_NO_LONGER_VALID:   Der Datenblock ist nicht mehr gültig         #
 * #    LUT2_NOT_YET_VALID:     Der Datenblock ist noch nicht gültig         #
 * #    LUT2_NO_VALID_DATE:     Der Datenblock enthält kein Gültigkeitsdatum #
 * #    LUT2_BLOCK_NOT_IN_FILE: Die LUT-Datei enthält den Infoblock nicht    #
 * #                                                                         #
 * # Copyright (C) 2007 Michael Plugge <m.plugge@hs-mannheim.de>             #
 * ###########################################################################
 */

DLL_EXPORT int lut_info(char *lut_name,char **info1,char **info2,int *valid1,int *valid2)
{
   char *ptr,*ptr1,buffer[128];
   int i,j,ret;
   UINT4 v1,v2,current,cnt;
   time_t t;
   struct tm timebuf,*timeptr;
   FILE *in;

   t=time(NULL);
   timeptr=localtime_r(&t,&timebuf);
   current=(timeptr->tm_year+1900)*10000+(timeptr->tm_mon+1)*100+timeptr->tm_mday;  /* aktuelles Datum als JJJJMMTT */
#if DEBUG>0
   if(current_date)current=current_date;
#endif

      /* Gültigkeit des aktuell geladenen Datensatzes testen */
   if(!lut_name || !*lut_name){
      if((init_status&7)<7){
         if(info1)*info1=NULL;
         if(info2)*info2=NULL;
         if(valid1)*valid1=LUT2_NOT_INITIALIZED;
         if(valid2)*valid2=LUT2_NOT_INITIALIZED;
         return LUT2_NOT_INITIALIZED;
      }
      if(info1){
         if(!current_info)
            *info1=NULL;
         else{
            *info1=ptr=malloc(current_info_len+8192);
            sprintf(ptr,"%s\nin den Speicher geladene Blocks:\n   ",current_info);
            while(*ptr)ptr++;
            ptr1=*info1+current_info_len+8000;
            for(i=j=0;i<SET_OFFSET && ptr<ptr1;i++)if(lut2_block_status[i]==OK){
               while(*ptr)ptr++;
               if(j>0)*ptr++=',';
               *ptr++=' ';
               sprintf(ptr,"%s",lut_block_name1[i]);
               j++;
            }
            while(*ptr)ptr++;
            *ptr++='\n';
            *ptr++=0;
            *info1=realloc(*info1,ptr-*info1+10);
         }
      }
      if(valid1){
         if(!current_v1 || !current_v2) /* (mindestens) ein Datum fehlt */
            *valid1=LUT2_NO_VALID_DATE;
         else if(current>=current_v1 && current<=current_v2)
            *valid1=LUT2_VALID;
         else if(current<current_v1)
            *valid1=LUT2_NOT_YET_VALID;
         else if(current>current_v2)
            *valid1=LUT2_NO_LONGER_VALID;
      }
      if(info2)*info2=NULL;
      if(valid2)*valid2=LUT2_BLOCK_NOT_IN_FILE;
      return OK;
   }

      /* Datensätze aus einer Datei */
   if(info1)*info1=NULL;
   if(info2)*info2=NULL;
   if(valid1)*valid1=0;
   if(valid2)*valid2=0;
   if(!(in=fopen(lut_name,"rb")))return FILE_READ_ERROR;

      /* zunächst die LUT-Version testen */
   ptr=fgets(buffer,128,in);
   while(*ptr && *ptr!='\n')ptr++;
   *--ptr=0;
   if(!strcmp(buffer,"BLZ Lookup Table/Format 1.")){
      fclose(in);
      return LUT1_FILE_USED; /* alte LUT-Datei */
   }
   if(strcmp(buffer,"BLZ Lookup Table/Format 2.")){
      fclose(in);
      return INVALID_LUT_FILE; /* keine LUT-Datei */
   }
   rewind(in);

      /* Infoblocks lesen: 1. Infoblock */
   if((ret=read_lut_block_int(in,0,LUT2_INFO,&cnt,&ptr))==OK){
RBIX(ptr,LUT2_INFO);
      *(ptr+cnt)=0;
      if(valid1){
         for(ptr1=ptr,v1=v2=0;*ptr1 && *ptr1!='\n' && !isdigit(*ptr1);ptr1++);
         if(*ptr1 && *ptr1!='\n'){
            v1=strtoul(ptr1,NULL,10);              /* Anfangsdatum der Gültigkeit */
            if(*ptr1 && *ptr1!='\n'){
               while(*ptr1 && *ptr1!='\n' && *ptr1++!='-');  /* Endedatum suchen */
               if(*ptr1)v2=strtoul(ptr1,NULL,10);  /* Endedatum der Gültigkeit */
            }
         }
         if(!v1 || !v2) /* (mindestens) ein Datum fehlt */
            *valid1=LUT2_NO_VALID_DATE;
         else if(current>=v1 && current<=v2)
            *valid1=LUT2_VALID;
         else if(current<v1)
            *valid1=LUT2_NOT_YET_VALID;
         else if(current>v2)
            *valid1=LUT2_NO_LONGER_VALID;
      }
      if(info1)
         *info1=ptr;
      else
         FREE(ptr);
   }
   else{
      ret=get_lut_info2(lut_name,&i,&ptr,NULL,NULL);
      if(info1)
         *info1=ptr;
      else
         FREE(ptr);
      if(valid1){
         if(i<3)
            *valid1=LUT1_SET_LOADED;
         else
            *valid1=ret;
      }
      if(info2)*info2=NULL;
      if(valid2)*valid2=0;
      return ret;
   }

      /* Infoblocks lesen: 1. Infoblock */
   if((ret=read_lut_block_int(in,0,LUT2_2_INFO,&cnt,&ptr))==OK){
RBIX(ptr,LUT2_2_INFO);
      *(ptr+cnt)=0;
      if(valid2){
         for(ptr1=ptr,v1=v2=0;*ptr1 && *ptr1!='\n' && !isdigit(*ptr1);ptr1++);
         if(*ptr1 && *ptr1!='\n'){
            v1=strtoul(ptr1,NULL,10);              /* Anfangsdatum der Gültigkeit */
            if(*ptr1 && *ptr1!='\n'){
               while(*ptr1 && *ptr1!='\n' && *ptr1++!='-');  /* Endedatum suchen */
               if(*ptr1)v2=strtoul(ptr1,NULL,10);  /* Endedatum der Gültigkeit */
            }
         }
         if(!v1 || !v2) /* (mindestens) ein Datum fehlt */
            *valid2=LUT2_NO_VALID_DATE;
         else if(current>=v1 && current<=v2)
            *valid2=LUT2_VALID;
         else if(current<v1)
            *valid2=LUT2_NOT_YET_VALID;
         else if(current>v2)
            *valid2=LUT2_NO_LONGER_VALID;
      }
      if(info2)
         *info2=ptr;
      else
         FREE(ptr);
   }
   else{
      if(info2)*info2=NULL;
      if(valid2)*valid2=ret;
   }
   fclose(in);
   return OK;
}

/* Funktion get_lut_info2() +§§§1 */
/* ###########################################################################
 * # get_lut_info2(): Prolog, Infozeilen und Version einer LUT-Datei holen   #
 * #                                                                         #
 * # Die Funktion liest den Prolog einer LUT-Datei und wertet ihn aus; es    #
 * # werden verschiedene Variablen zurückgegeben, in denen die Prolog-Daten  #
 * # enthalten sind. Die Funktion allokiert Speicher für den Prolog; dieser  #
 * # muß vom aufrufenden Programm wieder freigegeben werden.                 #
 * #                                                                         #
 * # Die Werte der Info-Zeile und User-Info Zeile werden nur zurückgegeben,  #
 * # falls auch eine Variable für prolog spezifiziert ist. Es wird Speicher  #
 * # allokiert, der der Variablen prolog zugewiesen wird; für info und       #
 * # user_info wird kein eigener Speicherbereich benutzt.                    #
 * #                                                                         #
 * # Diese Funktion stammt noch aus dem alten Interface und liefert nur die  #
 * # Werte aus dem Prolog der LUT-Datei. Eine ähnliche Funktion ist für das  #
 * # neue LUT-Format ist lut_info(); diese extrahiert die Infoblocks der     #
 * # Datei und liefert auch eine Aussage über die Gültigkeit der Daten.      #
 * #                                                                         #
 * # Parameter:                                                              #
 * #    lut_name:  Name der LUT-Datei                                        #
 * #    info:      Die Variable wird auf die Infozeile gesetzt               #
 * #    version_p: Variablenpointer für Rückgabe der LUT-Version             #
 * #    prolog_p:  Variablenpointer für Rückgabe des Prologs (per malloc!!)  #
 * #    info_p:    Variablenpointer für Rückgabe des Info-Strings            #
 * #    user_info_p: Variablenpointer für Rückgabe des User-Info-Strings     #
 * #                                                                         #
 * # Copyright (C) 2007 Michael Plugge <m.plugge@hs-mannheim.de>             #
 * ###########################################################################
 */

DLL_EXPORT int get_lut_info2(char *lut_name,int *version_p,char **prolog_p,char **info_p,char **user_info_p)
{
   char *buffer,*ptr,*sptr,*info="",*user_info="",name_buffer[LUT_PATH_LEN];
   int i,j,k,buflen,version,zeile;
   unsigned long offset1,offset2;
   struct stat s_buf;
   FILE *lut;

   if(prolog_p)*prolog_p=NULL;
   if(info_p)*info_p=NULL;
   if(user_info_p)*user_info_p=NULL;

      /* falls keine LUT-Datei angegeben wurde, die Suchpfade und Defaultnamen durchprobieren */
   if(!lut_name || !*lut_name){
      for(i=0,k=-1,lut_name=name_buffer;i<lut_name_cnt && k==-1;i++){
         for(j=0;j<lut_searchpath_cnt;j++){
#if _WIN32>0
            snprintf(lut_name,LUT_PATH_LEN,"%s\\%s",lut_searchpath[j],default_lutname[i]);
#else
            snprintf(lut_name,LUT_PATH_LEN,"%s/%s",lut_searchpath[j],default_lutname[i]);
#endif
            if(!(k=stat(lut_name,&s_buf)))break;
         }
      }
      if(k==-1)return NO_LUT_FILE;  /* keine Datei gefunden */
   }

   stat(lut_name,&s_buf);
   buflen=s_buf.st_size;
   if(!(buffer=malloc(buflen)))return ERROR_MALLOC;
   if(!(lut=fopen(lut_name,"rb"))){
      FREE(buffer);
      return FILE_READ_ERROR;
   }
   for(zeile=version=0,ptr=buffer;!feof(lut);){
      if(!fgets(ptr,buflen,lut))return FILE_READ_ERROR;  /* LUT-Datei zeilenweise einlesen */
      if(!version && !strncmp(buffer,"BLZ Lookup Table/Format 1.0\n",28))version=1;
      if(!version && !strncmp(buffer,"BLZ Lookup Table/Format 1.1\n",28))version=2;
      if(!version && !strncmp(buffer,"BLZ Lookup Table/Format 2.0\n",28))version=3;
      if(++zeile==2)info=ptr;
      if(version==3 && !strncmp(ptr,"DATA\n",5)){  /* Ende des Prologs (LUT 2.0), Nullbyte anhängen */
         *ptr++=0;
         break;
      }
      for(;*ptr;ptr++)buflen--;  /* ptr hinter das Zeilenende setzen, buflen decrement für fgets */
      if(zeile==2){
         if(version>1 && *(ptr-2)=='\\')  /* User-Infozeile vorhanden */
            user_info=ptr;
         else{  /* keine User-Infozeile */
            user_info="";
            if(version<3){ /* bei Fileversion 2.0 kommen noch einige Prolog-Daten */
               *ptr++=0;   /* Ende des Prologs (LUT 1.0/1.1), Nullbyte anhängen */
               break;
            }
         }
      }
      if(version==2 && zeile==3){
         *ptr++=0;
         break;
      }
   }
   *ptr++=0;
   if(version_p)*version_p=version;

   if(!prolog_p)  /* keine Rückgabevariable für Prolog => buffer wieder freigeben */
      FREE(buffer);
   else{
         /* sicherstellen, daß in buffer genügend Platz ist für Prolog und Info/User-Info */
      if(s_buf.st_size<(ptr-buffer)*2+10)buffer=realloc(buffer,(ptr-buffer)*2+10);

         /* Variablen setzen, dann info und user_info kopieren */
      for(sptr=info,info=ptr;*sptr && *sptr!='\n' && *sptr!='\\';)*ptr++=*sptr++;
      *ptr++=0;
      for(sptr=user_info,user_info=ptr;*sptr && *sptr!='\n' && *sptr!='\\';)*ptr++=*sptr++;
      *ptr++=0;

         /* realloc liefert auf einigen Systemen (FreeBSD u.a.) auch bei
          * Verkleinerung des Buffers eine andere Adresse zurück. Daher werden
          * info_p und user_info_p zunächst nur als Offset relativ zu buffer
          * als Basisadresse genommen, und nach dem realloc auf absolute
          * Adressen gesetzt. Dieser Fehler [perl5.11.0 in free(): error: page
          * is already free] tauchte in den Versionen 2.91 und 2.92 in etlichen
          * Tests auf CPAN auf; er dürfte jetzt behoben sein.
          */
      offset1=(info-buffer);
      offset2=(user_info-buffer);
      buffer=realloc(buffer,(ptr-buffer+10)); /* überflüssigen Speicher wieder freigeben */

         /* nun die absoluten Adressen eintragen */
      *prolog_p=buffer;
      if(info_p)*info_p=buffer+offset1;
      if(user_info_p)*user_info_p=buffer+offset2;
   }
   fclose(lut);
   return OK;
}

/* Funktion copy_lutfile() +§§§1 */
/* ###########################################################################
 * # Die Funktion kopiert eine LUT-Datei, wobei alle obsoleten Blocks        #
 * # weggelassen werden. Falls in einer LUT-Datei mehrere Blocks desselben   #
 * # Typs enthalten sind, wird normalerweise immer der letzte (jüngste)      #
 * # benutzt; diese Funktion säubert somit eine LUT-Datei von Altlasten.     #
 * # Außerdem kann die Anzahl Slots verändert (vergrößert oder verkleinert)  #
 * # werden.                                                                 #
 * #                                                                         #
 * # Copyright (C) 2007 Michael Plugge <m.plugge@hs-mannheim.de>             #
 * ###########################################################################
 */

DLL_EXPORT int copy_lutfile(char *old_name,char *new_name,int new_slots)
{
   char *data,*prolog;
   int i,retval,version,last_slot,slotdir[MAX_SLOTS];
   UINT4 slot_cnt,typ,len;
   FILE *lut1=NULL,*lut2;

   if(!init_status&1)init_atoi_table();

      /* Dateiprolog einlesen */
   if((retval=get_lut_info2(old_name,&version,&prolog,NULL,NULL))!=OK)return retval;
   if(version<3)retval=INVALID_LUT_VERSION;  /* kopieren erst ab LUT-Version 2.0 möglich */
   if(retval==OK && !(lut1=fopen(old_name,"rb")))retval=FILE_READ_ERROR;
   if(retval==OK)retval=lut_dir(lut1,0,&slot_cnt,NULL,NULL,NULL,NULL,slotdir);
   if(!new_slots)new_slots=slot_cnt;
   if(retval==OK)retval=create_lutfile_int(new_name,prolog,new_slots,&lut2);  /* neue LUT-Datei anlegen */
   FREE(prolog);
   if(retval!=OK)return retval;

      /* Liste sortieren, damit jeder Eintrag nur einmal geschrieben wird */
   qsort(slotdir,slot_cnt,sizeof(int),sort_int);
   for(last_slot=-1,i=0;i<slot_cnt;i++)if((typ=slotdir[i]) && typ!=last_slot){
      read_lut_block_int(lut1,0,typ,&len,&data);
RBIX(data,typ);
      write_lut_block_int(lut2,typ,len,data);
      FREE(data);
      last_slot=typ;
   }
   fclose(lut2);
   return OK;
}

/* Funktion kto_check_init2() +§§§1 */
/* ###########################################################################
 * # Diese Funktion ist die Minimalversion zur Initialisierung der           #
 * # konto_check Bibliothek. Sie hat als Parameter nur den Dateinamen (der   #
 * # natürlich auch NULL sein kann); es werden dann alle Blocks des aktuellen#
 * # Sets geladen. Der Rückgabewert ist daher oft -38 (nicht alle Blocks     #
 * # geladen), da die LUT-Datei nicht unbedingt alle Blocks enthält :-).     #
 * #                                                                         #
 * # Copyright (C) 2008 Michael Plugge <m.plugge@hs-mannheim.de>             #
 * ###########################################################################
 */

DLL_EXPORT int kto_check_init2(char *lut_name)
{
   return kto_check_init_p(lut_name,9,0,0);
}

/* Funktion kto_check_init_p() +§§§1 */
/* ###########################################################################
 * # Dies ist eine etwas vereinfachte Funktion für die Initialisierung der   #
 * # konto_check Bibliothek, die vor allem für den Aufruf aus Perl und PHP   #
 * # gedacht ist. Die Bankleitzahlen, Prüfziffern und Anzahl Filialen werden #
 * # in jedem Fall geladen; außerdem sind noch 10 verschiedene Level mit     #
 * # unterschiedlichen Blocks definiert (lut_set_0 ... lut_set_9), die über  #
 * # einen skalaren Parameter ausgewählt werden können.                      #
 * #                                                                         #
 * # Copyright (C) 2008 Michael Plugge <m.plugge@hs-mannheim.de>             #
 * ###########################################################################
 */

DLL_EXPORT int kto_check_init_p(char *lut_name,int required,int set,int incremental)
{
   int i,j;
   int *rq1,rq2[MAX_SLOTS+1];

   lut_init_level=required;
   switch(required){
      case 0:  rq1=lut_set_0; break;
      case 1:  rq1=lut_set_1; break;
      case 2:  rq1=lut_set_2; break;
      case 3:  rq1=lut_set_3; break;
      case 4:  rq1=lut_set_4; break;
      case 5:  rq1=lut_set_5; break;
      case 6:  rq1=lut_set_6; break;
      case 7:  rq1=lut_set_7; break;
      case 8:  rq1=lut_set_8; break;
      case 9:  rq1=lut_set_9; break;
      default: rq1=lut_set_9; break;
   }
   i=0;
   rq2[i++]=LUT2_BLZ;
   rq2[i++]=LUT2_PZ;
   rq2[i++]=LUT2_FILIALEN;
   for(j=0;i<MAX_SLOTS && rq1[j];)rq2[i++]=rq1[j++];
   rq2[i]=0;
   if(init_status<7)incremental=0; /* noch nicht initialisiert, inkrementell geht nicht */
   return kto_check_init(lut_name,rq2,NULL,set,incremental);
}

/* @@ Funktion lut2_status() +§§§1 */
/* ###########################################################################
 * #                                                                         #
 * #                                                                         #
 * #                                                                         #
 * #                                                                         #
 * #                                                                         #
 * #                                                                         #
 * #                                                                         #
 * #                                                                         #
 * #                                                                         #
 * # Copyright (C) 2008 Michael Plugge <m.plugge@hs-mannheim.de>             #
 * ###########################################################################
 */

DLL_EXPORT int *lut2_status(void)
{
  return lut2_block_status;
}

/* Funktion get_lut_id() +§§§1 */
/* ###########################################################################
 * # Diese Funktion liefert die Datei-ID einer LUT-Datei zurück. Sie wird    #
 * # benutzt, um bei einer inkrementellen Initialisierung zu gewährleisten,  #
 * # daß die Initialisierung auch von demselben Datensatz erfolgt, da        #
 * # ansonsten Inkonsitenzen zu erwarten wären. Die Datei-ID wird für jeden  #
 * # Datensatz im zugehörigen Infoblock gespeichert; im Prolog der Datei     #
 * # findet sich zwar auch eine Datei-ID, dies ist jedoch nur die des ersten #
 * # Datensatzes.                                                            #
 * #                                                                         #
 * # Der Parameter id sollte auf einen Speicherbereich von mindestens 33 Byte#
 * # zeigen. Die Datei-ID wird in diesen Speicher geschrieben.               #
 * #                                                                         #
 * # Copyright (C) 2008 Michael Plugge <m.plugge@hs-mannheim.de>             #
 * ###########################################################################
 */

DLL_EXPORT int get_lut_id(char *lut_name,int set,char *id)
{
   char *info,*info1,*info2,*ptr,*dptr;
   int ret,valid,valid1,valid2;

   *id=0;
   info=NULL;
   if(!lut_name || !*lut_name){
      if(lut_id_status==LUT1_SET_LOADED)return LUT1_FILE_USED;
      if(id)strncpy(id,lut_id,33);
      if(*lut_id)
         return OK;
      else
         return FALSE;
   }
   else
      switch(set){
         case 0:  /* beide Sets laden, und das gültige nehmen; falls keines gültig ist, Set 1 */
            ret=lut_info(lut_name,&info1,&info2,&valid1,&valid2);
            if(valid1==LUT1_SET_LOADED)return LUT1_FILE_USED;
            if(valid1==LUT2_VALID){
               info=info1;
               valid=valid1;
               FREE(info2);
            }
            else if(valid2==LUT2_VALID){
               info=info2;
               valid=valid2;
               FREE(info1);
            }
            else{
               info=info1;
               valid=valid1;
               FREE(info2);
            }
            break;

         case 1:  /* nur Set 1 */
            ret=lut_info(lut_name,&info1,NULL,&valid1,NULL);
            if(valid1==LUT1_SET_LOADED)return LUT1_FILE_USED;
            FREE(info2);
            if(info1)
               info=info1;
            else
               return FALSE;
            valid=valid2;
            break;

         case 2:  /* nur Set 2 */
            ret=lut_info(lut_name,NULL,&info2,NULL,&valid2);
            if(valid2==LUT1_SET_LOADED)return LUT1_FILE_USED;
            FREE(info1);
            if(info2)
               info=info2;
            else
               return FALSE;
            valid=valid2;
            break;

         default: /* Fehler */
            FREE(info1);
            FREE(info2);
            return INVALID_SET;
      }

   if(info && id)for(ptr=info;*ptr;){
      while(*ptr && *ptr++!='\n');
      if(!strncmp(ptr,"Datei-ID (zuf",13)){
         while(*ptr && *ptr++!='\n');
         for(dptr=id;(*dptr=*ptr) && *ptr && *ptr++!='\n';dptr++);
         if(*dptr=='\n')*dptr=0;
         FREE(info);
         return OK;
      }
   }
   FREE(info);
   return FALSE;
}

/* Funktion lut_init()  */
/* ###########################################################################
 * # Diese Funktion dient dazu, die konto_check Bibliothek zu initialisieren #
 * # und bietet ein (im Gegensatz zu kto_check_init) stark vereinfachtes     #
 * # Benutzerinterface (teilweise entlehnt von kto_check_init_p). Zunächst   #
 * # wird getestet, ob die Bibliothek schon mit der angegebenen Datei (bzw.  #
 * # genauer mit dem gewünschten Datensatz aus der Datei) initialisiert      #
 * # wurde (mittels der Datei-ID aus dem Infoblock des gewählten bzw.        #
 * # gültigen Sets). Falls die Datei-IDs nicht übereinstimmen, wird eine     #
 * # Neuinitialisierung gemacht, andernfalls (nur falls notwendig) eine      #
 * # inkrementelle Initialisierung, um noch benötigte Blocks nachzuladen.    #
 * # Falls schon alle gewünschten Blocks geladen sind, wird nichts gemacht.  #
 * #                                                                         #
 * # Copyright (C) 2008 Michael Plugge <m.plugge@hs-mannheim.de>             #
 * ###########################################################################
 */

DLL_EXPORT int lut_init(char *lut_name,int required,int set)
{
   char file_id[36];
   int must_init,incremental;

   must_init=0;
   incremental=1;
   if(get_lut_id(lut_name,set,file_id)!=OK || !*file_id || strcmp(file_id,lut_id)){
      incremental=0;
      lut_cleanup();
      must_init=1;
   }
   if(!must_init && required<=lut_init_level)return OK;  /* schon initialisiert */
   return kto_check_init_p(lut_name,required,set,incremental);
}

/* Funktion kto_check_init() +§§§1 */
/* ###########################################################################
 * # Die Funktion kto_check_init() ist die eigentliche Funktion zur          #
 * # Initialisierung der konto_check Bibliothek; alle abgeleiteten Funktionen#
 * # greifen auf diese Funktion zurück. Sie ist sehr flexibel, aber beim     #
 * # Glue Code für andere Sprachen machen die Parameter required (INT-Array  #
 * # der gewünschten Blocks) und status (Pointer auf ein INT-Array, in dem   #
 * # der Status des jeweiligen Blocks zurückgegeben wird) manchmal etwas     #
 * # Probleme; daher gibt es noch einige andere Initialisierungsfunktionen   #
 * # mit einfacherem Aufrufinterface, wie lut_init() oder kto_check_init_p().#
 * #                                                                         #
 * # Copyright (C) 2008 Michael Plugge <m.plugge@hs-mannheim.de>             #
 * ###########################################################################
 */

DLL_EXPORT int kto_check_init(char *lut_name,int *required,int **status,int set,int incremental)
{
   char *ptr,*dptr,*xptr,*data,*eptr,*prolog,*info,*user_info,*hs=NULL,*info1,*info2,*ci=NULL,name_buffer[LUT_PATH_LEN];
   int b,h,i,j,k,v1,v2,retval,release_data,alles_ok,slotdir[MAX_SLOTS],*iptr,*rptr,xrequired[MAX_SLOTS];
   UINT4 len,typ,typ1,set_offset=0,slot_cnt;
   FILE *lut;
   struct stat s_buf;

   INITIALIZE_WAIT;     /* zunächst testen, ob noch eine andere Initialisierung läuft (z.B. in einem anderen Thread) */
   init_in_progress=1;  /* Lockflag für Tests und Initialierung setzen */
   init_status|=8;      /* init_status wird bei der Prüfung getestet */
   usleep(10);
   if(init_status&16){
      init_in_progress=0;        /* Flag für Aufräumaktion rücksetzen */
      return INIT_FATAL_ERROR;   /* Aufräumaktion parallel gelaufen; alles hinwerfen */
   }

   if(!required)required=lut_set_9;   /* falls nichts angegeben, alle Felder einlesen */

      /* falls keine LUT-Datei angegeben wurde, die Suchpfade und Defaultnamen durchprobieren */
   if(!lut_name || !*lut_name){
      for(j=0,k=-1;j<lut_searchpath_cnt && k==-1;j++){
         for(i=0,lut_name=name_buffer;i<lut_name_cnt;i++){
#if _WIN32>0
            snprintf(lut_name,LUT_PATH_LEN,"%s\\%s",lut_searchpath[j],default_lutname[i]);
#else
            snprintf(lut_name,LUT_PATH_LEN,"%s/%s",lut_searchpath[j],default_lutname[i]);
#endif
            if(!(k=stat(lut_name,&s_buf)))break;
         }
      }
      if(k==-1)return NO_LUT_FILE;  /* keine Datei gefunden */
   }

      /* falls schon einmal initialisiert wurde (BLZ und PZ-Methoden gelesen),
       * eine Millisekunde warten, damit evl. laufende Tests sicher beendet
       * sind.
       */
   if((init_status&6)==6)usleep(1000);
   if(!incremental){
      lut_cleanup(); /* falls nicht inkrementelles init, alle bisher allokierten Variablen freigeben */
      if(!(init_status&1))init_atoi_table();
      init_status=1; /* init_status löschen, nur Variablen */
   }
   if(status)*status=lut2_block_status;   /* Rückgabe des Statusarrays, falls gewünscht */

      /* Info-Block holen und merken */
    if(lut_info(lut_name,&info1,&info2,&v1,&v2)==OK){
      if(!set){
         if(v1==LUT2_VALID){
            lut_id_status=v1;
            set=1;
         }
         else if(v2==LUT2_VALID){
            lut_id_status=v2;
            set=2;
         }
         else if(v1==LUT2_NO_VALID_DATE){
            lut_id_status=v1;
            set=1;
         }
         else if(v2==LUT2_NO_VALID_DATE){
            lut_id_status=v2;
            set=2;
         }
         else{
            lut_id_status=v1;
            set=1;
         }
      }
      if(set==1){
         if(incremental)
            ci=info1;
         else{
            FREE(current_info);
            current_info=info1;
         }
         FREE(info2);
         set_offset=0;
      }
      else{
         if(incremental)
            ci=info2;
         else{
            FREE(current_info);
            current_info=info2;
         }
         FREE(info1);
         set_offset=SET_OFFSET;
      }
      if(!current_info || (incremental && !ci)){
         FREE(ci);
         init_in_progress=0;
         init_status&=7;
         return LUT2_BLOCK_NOT_IN_FILE;
      }

            /* Beim inkrementellen Initialisieren den Prolog der initial
             * geladenen LUT-Datei (in current_info) mit dem der aktuell
             * angegebenen Datei (in ci) vergleichen. Die beiden müssen gleich
             * sein, ansonsten erfolgt ein Abbruch der Initialisierung, da bei
             * einer Initialisierung aus verschiedenen Dateien Inkonsistenzen
             * zu erwarten sind.
             */
      if(incremental){
         if(strcmp(ci,current_info)){
            init_in_progress=0;
            init_status&=7;
            FREE(ci);
            return INCREMENTAL_INIT_FROM_DIFFERENT_FILE;
         }
      }
      else{
         current_info_len=strlen(current_info);
         for(ptr=current_info,current_v1=current_v2=0;*ptr && *ptr!='\n' && !isdigit(*ptr);ptr++);
         if(*ptr && *ptr!='\n'){
            current_v1=strtoul(ptr,NULL,10);             /* Anfangsdatum der Gültigkeit */
            if(*ptr && *ptr!='\n'){
               while(*ptr && *ptr!='\n' && *ptr++!='-'); /* Endedatum suchen */
               if(*ptr)current_v2=strtoul(ptr,NULL,10);  /* Endedatum der Gültigkeit */
            }
         }
      }
      FREE(ci);
      for(ptr=current_info,*lut_id=0;*ptr;){  /* nun die ID der LUT-Datei merken */
         while(*ptr && *ptr++!='\n');
         if(!strncmp(ptr,"Datei-ID (zuf",13)){
            while(*ptr && *ptr++!='\n');
               /* LUT-ID in den statischen Buffer kopieren */
            for(dptr=lut_id;(*dptr=*ptr) && *ptr && *ptr++!='\n';dptr++);
            if(*dptr=='\n')*dptr=0;
         }
      }
    }
   else{
      if(incremental)return INCREMENTAL_INIT_NEEDS_INFO;
      current_info=NULL;
      *lut_id=0;
      current_info_len=current_v1=current_v2=0;
      if(!set)set=1; /* kein Gültigkeitsdatum vorhanden, defaultmäßig primären Datensatz nehmen */
   }

      /* zunächst muß zwingend die die BLZ und die Anzahl der Filialen
       * eingelesen werden (wegen der Anzahl Datensätze) */
   *xrequired=LUT2_BLZ+set_offset;
   *(xrequired+1)=LUT2_FILIALEN+set_offset;
   for(iptr=required,rptr=xrequired+2;*iptr;iptr++)
      if(*iptr>SET_OFFSET)
         *rptr++=*iptr-SET_OFFSET+set_offset;
      else
         *rptr++=*iptr+set_offset;
   *rptr=0;

   if(!incremental){ /* dieser Teil wird nur beim ersten Einlesen benötigt */

      /* Prolog und Infozeilen der Datei holen und nach own_buffer kopieren */
      if((retval=get_lut_info2(lut_name,&lut_version,&prolog,&info,&user_info))!=OK){
         FREE(prolog);
         init_in_progress=0;
         return retval;
      }
      FREE(own_buffer);
      own_buffer=malloc(strlen(prolog)+strlen(info)+strlen(user_info)+10);
      for(lut_prolog=optr=own_buffer,ptr=prolog;(*optr++=*ptr++););
      for(ptr=info,lut_sys_info=optr;(*optr++=*ptr++););
      for(ptr=user_info,lut_user_info=optr;(*optr++=*ptr++););
      FREE(prolog);
      if(lut_version<3){
         retval=read_lut(lut_name,&lut2_cnt_hs);
         if(retval==OK){
            lut2_cnt=lut2_cnt_hs;
            lut2_block_status[LUT2_BLZ]=lut2_block_status[LUT2_PZ]=OK;
            init_status|=6;
            init_status&=7;
            init_in_progress=0;
            return LUT1_SET_LOADED;
         }
         else{
            init_in_progress=0;
            return retval;
         }
      }
   }

   if(!(lut=fopen(lut_name,"rb"))){
      init_in_progress=0;
      return FILE_READ_ERROR;
   }
   if((retval=lut_dir(lut,0,&slot_cnt,NULL,NULL,NULL,NULL,slotdir))!=OK){
      fclose(lut);
      init_in_progress=0;
      return retval;
   } 
   for(rptr=xrequired,alles_ok=1;*rptr;){  /* versuchen, die gewünschten Blocks einzulesen */
      typ=*rptr++;
      if(typ>SET_OFFSET)
         typ1=typ-SET_OFFSET;
      else
         typ1=typ;
      if(lut2_block_status[typ]==OK)continue;   /* jeden Block nur einmal einlesen */
      retval=read_lut_block_int(lut,0,typ,&len,&data);
RBIX(data,typ);

      switch(retval){
         case LUT_CRC_ERROR:
         case LUT2_Z_BUF_ERROR:
         case LUT2_Z_MEM_ERROR:
         case LUT2_Z_DATA_ERROR:

               /* Fehler bei einem Block; eintragen, dann weitere Blocks einlesen */
            lut2_block_status[typ]=lut2_block_status[typ1]=retval;
            alles_ok=lut2_block_len[typ]=0;
            lut2_block_data[typ]=NULL;
            continue;

         case LUT2_BLOCK_NOT_IN_FILE:

               /* Sonderfall LUT2_NAME und LUT2_NAME_KURZ: die beiden Blocks
                * können auch gemeinsam in LUT2_NAME_NAME_KURZ enthalten sein;
                * versuchen, diesen Block einzulesen; umgekehrt genauso.
                */
            if(typ==LUT2_NAME_NAME_KURZ){
               *--rptr=LUT2_NAME_KURZ; /* beim nächsten Block den Kurznamen einlesen */
               typ=LUT2_NAME;
               FREE(data);
               i=read_lut_block_int(lut,0,LUT2_NAME,&len,&data);
RBIX(data,LUT2_NAME);
               if(i==OK){  /* was gefunden; eintragen und Block verarbeiten */
                  lut2_block_status[typ]=lut2_block_status[typ1]=retval;
                  lut2_block_len[typ]=lut2_block_len[typ1]=len;
                  lut2_block_data[typ]=lut2_block_data[typ1]=data;
                  break;
               }
            }
            if(typ==LUT2_2_NAME_NAME_KURZ){ /* wie oben, nur sekundärer Datenblock */
               *--rptr=LUT2_2_NAME_KURZ; /* beim nächsten Block den Kurznamen einlesen */
               typ=LUT2_2_NAME;
               FREE(data);
               i=read_lut_block_int(lut,0,LUT2_2_NAME,&len,&data);
RBIX(data,LUT2_2_NAME);
               if(i==OK){  /* was gefunden; eintragen und Block verarbeiten */
                  lut2_block_status[typ]=lut2_block_status[typ1]=retval;
                  lut2_block_len[typ]=lut2_block_len[typ1]=len;
                  lut2_block_data[typ]=lut2_block_data[typ1]=data;
                  break;
               }
            }
            if(typ==LUT2_NAME || typ==LUT2_NAME_KURZ){
               FREE(data);
               i=read_lut_block_int(lut,0,LUT2_NAME_NAME_KURZ,&len,&data);
RBIX(data,LUT2_NAME_NAME_KURZ);
               if(i==OK){  /* was gefunden; Typ ändern, dann weiter wie bei OK */
                  typ=LUT2_NAME_NAME_KURZ;
                  lut2_block_status[typ]=lut2_block_status[typ1]=retval;
                  lut2_block_len[typ]=lut2_block_len[typ1]=len;
                  lut2_block_data[typ]=lut2_block_data[typ1]=data;
                  break;
               }
            }
            if(typ==LUT2_2_NAME || typ==LUT2_2_NAME_KURZ){
               FREE(data);
               i=read_lut_block_int(lut,0,LUT2_2_NAME_NAME_KURZ,&len,&data);
RBIX(data,LUT2_2_NAME_NAME_KURZ);
               if(i==OK){  /* was gefunden; Typ ändern, dann weiter wie bei OK */
                  typ=LUT2_2_NAME_NAME_KURZ;
                  lut2_block_status[typ]=lut2_block_status[typ1]=retval;
                  lut2_block_len[typ]=lut2_block_len[typ1]=len;
                  lut2_block_data[typ]=lut2_block_data[typ1]=data;
                  break;
               }
            }
               /* Fehler bei dem Block; eintragen, dann weitere Blocks einlesen */
            lut2_block_status[typ]=lut2_block_status[typ1]=retval;
            alles_ok=lut2_block_len[typ]=lut2_block_len[typ1]=0;
            lut2_block_data[typ]=lut2_block_data[typ1]=NULL;
            continue;

         case LUT2_FILE_CORRUPTED:
         case ERROR_MALLOC:
               /* fatale Fehler, Einlesen abbrechen */
            lut2_block_status[typ]=lut2_block_status[typ1]=retval;
            alles_ok=lut2_block_len[typ]=lut2_block_len[typ1]=0;
            lut2_block_data[typ]=lut2_block_data[typ1]=NULL;
            init_in_progress=0;
            return retval;

         case OK:
            lut2_block_status[typ]=lut2_block_status[typ1]=OK;
            lut2_block_len[typ]=lut2_block_len[typ1]=len;
            lut2_block_data[typ]=lut2_block_data[typ1]=data;
            break;
      }

         /* nun werden die internen Blocks verarbeitet */
      switch(typ){
         case LUT2_BLZ:  /* Bankleitzahl */
         case LUT2_2_BLZ:
            release_data=1;
            ptr=data;
            C2UI(lut2_cnt_hs,ptr);  /* Anzahl der Datensätze (nur Hauptstellen) holen */
            C2UI(lut2_cnt,ptr);     /* Anzahl der Datensätze (gesamt) holen */
            init_status|=2;

            FREE(blz);
            FREE(hash);
            if(!(blz=calloc(lut2_cnt_hs+1,sizeof(int)))
                  || (!startidx && !(startidx=calloc(lut2_cnt_hs,sizeof(int))))
                  || !(hash=calloc(sizeof(short),HASH_BUFFER_SIZE)))
               lut2_block_status[typ]=lut2_block_status[typ1]=ERROR_MALLOC;
            else{
               for(i=0,eptr=data+len;ptr<eptr && i<lut2_cnt_hs;i++){
                  startidx[i]=i;
                  j=UI *ptr++;
                  switch(j){
                     case 254:
                        C2UI(j,ptr);
                        blz[i]=blz[i-1]+j;
                        break;
                     case 255:
                        C2UL(j,ptr);
                        blz[i]=j;
                        break;
                     default:
                        blz[i]=blz[i-1]+j;
                        break;
                  }
               }
               blz[lut2_cnt_hs]=999999999;
               for(i=0;i<HASH_BUFFER_SIZE;i++)hash[i]=lut2_cnt_hs;
               for(i=0;i<lut2_cnt_hs;i++){
                  b=blz[i];      /* b BLZ, h Hashwert */
                  k=b%10; h= hx8[k]; b/=10;
                  k=b%10; h+=hx7[k]; b/=10;
                  k=b%10; h+=hx6[k]; b/=10;
                  k=b%10; h+=hx5[k]; b/=10;
                  k=b%10; h+=hx4[k]; b/=10;
                  k=b%10; h+=hx3[k]; b/=10;
                  k=b%10; h+=hx2[k]; b/=10;
                  k=b%10; h+=hx1[k];
                  while(hash[h]!=lut2_cnt_hs)h++;
                  hash[h]=i;
               }
            }
            break;

         case LUT2_FILIALEN:  /* Anzahl Filialen */
         case LUT2_2_FILIALEN:
            release_data=1;
            FREE(filialen);
            if(!(filialen=calloc(len,sizeof(int)))
                   || (!startidx && !(startidx=calloc(lut2_cnt_hs,sizeof(int)))))
               lut2_block_status[typ]=lut2_block_status[typ1]=ERROR_MALLOC;
            else{
               for(i=j=0,ptr=data,eptr=data+len;i<len;i++){
                  startidx[i]+=j;
                  j+=(filialen[i]=UI *ptr++)-1;
               }
            }
            break;

         case LUT2_NAME:  /* Bezeichnung des Kreditinstitutes (ohne Rechtsform) */
         case LUT2_2_NAME:
            release_data=0;
            FREE(name);
            if(!(name=calloc(lut2_cnt,sizeof(char*))))
               lut2_block_status[typ]=lut2_block_status[typ1]=ERROR_MALLOC;
            else{
               FREE(name_data);
               for(i=0,name_data=ptr=data,eptr=data+len;ptr<eptr && i<lut2_cnt;i++){
                  if(*ptr==1)
                     hs=name[i]=++ptr;
                  else if(*ptr)
                     name[i]=ptr;
                  else
                     name[i]=hs;
                  while(*ptr++ && ptr<eptr);
               }
            }
            break;

         case LUT2_NAME_KURZ:  /* Kurzbezeichnung des Kreditinstitutes mit Ort (ohne Rechtsform) */
         case LUT2_2_NAME_KURZ:
            release_data=0;
            FREE(name_kurz);
            if(!(name_kurz=calloc(lut2_cnt,sizeof(char*))))
               lut2_block_status[typ]=lut2_block_status[typ1]=ERROR_MALLOC;
            else{
               FREE(name_kurz_data);
               for(i=0,name_kurz_data=ptr=data,eptr=data+len;ptr<eptr && i<lut2_cnt;i++){
                  name_kurz[i]=ptr;
                  while(*ptr++ && ptr<eptr);
               }
            }
            break;

        case LUT2_NAME_NAME_KURZ:  /* Name und Kurzname zusammen */
        case LUT2_2_NAME_NAME_KURZ:
            release_data=0;
            FREE(name);
            FREE(name_kurz);
            if(!(name=calloc(lut2_cnt,sizeof(char*))) || !(name_kurz=calloc(lut2_cnt,sizeof(char*))))
               lut2_block_status[typ]=lut2_block_status[typ1]=ERROR_MALLOC;
            else{
               FREE(name_name_kurz_data);
               for(i=0,name_name_kurz_data=ptr=data,eptr=data+len;ptr<eptr && i<lut2_cnt;i++){
                  if(*ptr==1)
                     hs=name[i]=++ptr;
                  else if(*ptr)
                     name[i]=ptr;
                  else
                     name[i]=hs;
                  while(ptr<eptr && *ptr++);
                  name_kurz[i]=ptr;
                  while(ptr<eptr && *ptr++);
               }
               lut2_block_status[LUT2_NAME]=OK;
               lut2_block_status[LUT2_NAME_KURZ]=OK;
            }
            break;

         case LUT2_PLZ:  /* Postleitzahl */
         case LUT2_2_PLZ:
            release_data=1;
            FREE(plz);
            if(!(plz=calloc(len/3,sizeof(int)))){
               lut2_block_status[typ]=lut2_block_status[typ1]=ERROR_MALLOC;
            }
            else{
               for(i=0,ptr=data,eptr=data+len;ptr<eptr;i++){
                  C2UM(j,ptr);
                  plz[i]=j;
               }
            }
            break;

         case LUT2_ORT:  /* Ort */
         case LUT2_2_ORT:
            release_data=0;
            FREE(ort);
            if(!(ort=calloc(lut2_cnt,sizeof(char*))))
               lut2_block_status[typ]=lut2_block_status[typ1]=ERROR_MALLOC;
            else{
               FREE(ort_data);
               for(i=0,ort_data=ptr=data,eptr=data+len;ptr<eptr && i<lut2_cnt;i++){
                  ort[i]=ptr;
                  while(*ptr++ && ptr<eptr);
               }
            }
            break;

         case LUT2_PAN:  /* Institutsnummer für PAN */
         case LUT2_2_PAN:
            release_data=1;
            FREE(pan);
            if(!(pan=calloc(lut2_cnt,sizeof(int)))){
               lut2_block_status[typ]=lut2_block_status[typ1]=ERROR_MALLOC;
            }
            else{
               for(i=0,ptr=data,eptr=data+len;i<lut2_cnt;i++){
                  C2UM(j,ptr);
                  pan[i]=j;
               }
            }
            break;

         case LUT2_BIC:  /* Bank Identifier Code - BIC */
         case LUT2_2_BIC:
            release_data=1;
            xptr="           ";
            FREE(bic);
            FREE(bic_buffer);
            if(!(bic_buffer=calloc(lut2_cnt+10,12)) || !(bic=calloc(lut2_cnt+10,sizeof(char*)))){
               lut2_block_status[typ]=lut2_block_status[typ1]=ERROR_MALLOC;
            }
            else{
               for(i=0,ptr=data,eptr=data+len,dptr=bic_buffer;ptr<eptr && i<lut2_cnt;i++){
                  bic[i]=((char *)(dptr-bic_buffer));
                  if(!*ptr){  /* Leerstring einsetzen (über xptr, s.o.) */
                     bic[i]=(char *)(xptr-bic_buffer);
                     ptr++;
                  }
                  else if(*ptr==1){  /* Flag für Landkennzeichen != DE; komplett kopieren */
                     for(j=0,ptr++;j<11;j++)*dptr++=*ptr++;
                  }
                  else{
                     *dptr++=*ptr++;
                     *dptr++=*ptr++;
                     *dptr++=*ptr++;
                     *dptr++=*ptr++;
                     *dptr++='D';
                     *dptr++='E';
                     *dptr++=*ptr++;
                     *dptr++=*ptr++;
                     *dptr++=*ptr++;
                     *dptr++=*ptr++;
                     *dptr++=*ptr++;
                  }
                  *dptr++=0;
               }
               bic_buffer=realloc(bic_buffer,(size_t)(dptr-bic_buffer)+10);
               for(j=0;j<i;j++)bic[j]=(bic_buffer+(unsigned long)bic[j]);
            }
            break;

         case LUT2_PZ:  /* Kennzeichen für Prüfzifferberechnungsmethode */
         case LUT2_2_PZ:
            release_data=1;
            FREE(pz_methoden);
            if(!(pz_methoden=calloc(len,sizeof(int))))
               lut2_block_status[typ]=lut2_block_status[typ1]=ERROR_MALLOC;
            else{
               for(i=0,ptr=data,eptr=data+len;ptr<eptr;i++)pz_methoden[i]=UI *ptr++;
               init_status|=4;
            }
            break;

         case LUT2_NR:  /* Nummer des Datensatzes */
         case LUT2_2_NR:
            release_data=1;
            FREE(bank_nr);
            if(!(bank_nr=calloc(len,sizeof(int))))
               lut2_block_status[typ]=lut2_block_status[typ1]=ERROR_MALLOC;
            else{
               for(i=0,ptr=data,eptr=data+len;ptr<eptr;i++){
                  C2UM(j,ptr);
                  bank_nr[i]=j;
               }
            }
            break;

         case LUT2_AENDERUNG:  /* Änderungskennzeichen */
         case LUT2_2_AENDERUNG:
            release_data=1;
            FREE(aenderung);
            if(!(aenderung=calloc(len,1)))
               lut2_block_status[typ]=lut2_block_status[typ1]=ERROR_MALLOC;
            else
               for(ptr=data,dptr=aenderung,eptr=data+len;ptr<eptr;)*dptr++=*ptr++;
            break;

         case LUT2_LOESCHUNG:  /* Hinweis auf eine beabsichtigte Bankleitzahllöschung */
         case LUT2_2_LOESCHUNG:
            release_data=1;
            FREE(loeschung);
            if(!(loeschung=calloc(len,1)))
               lut2_block_status[typ]=lut2_block_status[typ1]=ERROR_MALLOC;
            else
               for(ptr=data,dptr=loeschung,eptr=data+len;ptr<eptr;)*dptr++=*ptr++;
            break;

         case LUT2_NACHFOLGE_BLZ:  /* Hinweis auf Nachfolge-Bankleitzahl */
         case LUT2_2_NACHFOLGE_BLZ:
            release_data=1;
            FREE(nachfolge_blz);
            if(!(nachfolge_blz=calloc(len/4,sizeof(int))))
               lut2_block_status[typ]=lut2_block_status[typ1]=ERROR_MALLOC;
            else{
               for(i=0,ptr=data,eptr=data+len;ptr<eptr;i++){
                  C2UL(j,ptr);
                  nachfolge_blz[i]=j;
               }
            }
            break;

         default :   /* Benutzer-Datenblock: nicht verarbeiten, Daten stehenlassen */
            release_data=0;
            continue;   /* nächsten Block einlesen */
      }
      if(release_data){
         FREE(data); /* die (Roh-)Daten werden nicht mehr benötigt, Speicher freigeben */
         lut2_block_len[typ]=lut2_block_len[typ1]=0;
         lut2_block_data[typ]=lut2_block_data[typ1]=NULL;
      }
   }
   fclose(lut);
   init_in_progress=0;
   init_status&=7;
   if(alles_ok)
      return OK;
   else
      return LUT2_PARTIAL_OK;
}

/* Funktion lut_index() +§§§1 */
/* ###########################################################################
 * # lut_index(): Index einer BLZ in den internen Arrays bestimmen           #
 * #                                                                         #
 * # Copyright (C) 2007 Michael Plugge <m.plugge@hs-mannheim.de>             #
 * ###########################################################################
 */

static int lut_index(char *b)
{
   short *iptr;
   int n,h;

   if((init_status&7)!=7)return LUT2_NOT_INITIALIZED;   /* BLZ oder atoi_table noch nicht initialisiert */
   while(*b==' ' || *b=='\t')b++;   /* führende Blanks/Tabs entfernen */
   n= b8[UI *b]; h= h1[UI *b++];
   n+=b7[UI *b]; h+=h2[UI *b++];
   n+=b6[UI *b]; h+=h3[UI *b++];
   n+=b5[UI *b]; h+=h4[UI *b++];
   n+=b4[UI *b]; h+=h5[UI *b++];
   n+=b3[UI *b]; h+=h6[UI *b++];
   n+=b2[UI *b]; h+=h7[UI *b++];
   n+=b1[UI *b]; h+=h8[UI *b++];
   n+=b0[UI *b];     /* abfangen, wenn eine BLZ mehr als 8 Ziffern hat ist */

   if(n>=BLZ_FEHLER)return INVALID_BLZ_LENGTH;  /* nicht im BLZ-Array enthalten */
   if(blz[hash[h]]==n)return hash[h];           /* BLZ gefunden, Index zurückgeben */
   iptr=hash+h+1;

      /* die BLZs sind nach Größe sortiert, unbelegte Felder zeigen auf
       * blz[lut2_cnt_hs]. Dieser Wert ist mit MAX_INT belegt. Falls also die
       * BLZ, die einem Hashwert zugeordnet wird, größer als n ist, gibt es die
       * gesuchte Zahl im BLZ-Array nicht.
       */
   if(blz[*iptr]>n)return INVALID_BLZ;
   if(blz[*iptr]==n)return *iptr;
   if(blz[*++iptr]>n)return INVALID_BLZ;
   if(blz[*iptr]==n)return *iptr;

      /* bis hierhin dürften die meisten BLZs gefunden sein, der Rest in einer Schleife */
   while(1){
      if(blz[*++iptr]>n)return INVALID_BLZ;
      if(blz[*iptr]==n)return *iptr;
   }
}

/* Funktion lut_index_i() +§§§1 */
/* ###########################################################################
 * # lut_index_i(): Index einer BLZ in den internen Arrays bestimmen         #
 * #                (die BLZ liegt als Integerwert vor).                     #
 * #                Diese Funktion ist nicht so optimiert wie lut_index(),   #
 * #                da sie nicht in zeitkritischen Routinen benutzt wird.    #
 * #                                                                         #
 * # Copyright (C) 2009 Michael Plugge <m.plugge@hs-mannheim.de>             #
 * ###########################################################################
 */

static int lut_index_i(int b)
{
   short *iptr;
   int n,br,h;

   if((init_status&7)!=7)return LUT2_NOT_INITIALIZED;   /* BLZ oder atoi_table noch nicht initialisiert */
   if(b<10000000 || b>99999999)return INVALID_BLZ_LENGTH;  /* ungültig */
   n=b;
   br=b%10; b/=10; h= h8[br+'0'];
   br=b%10; b/=10; h+=h7[br+'0'];
   br=b%10; b/=10; h+=h6[br+'0'];
   br=b%10; b/=10; h+=h5[br+'0'];
   br=b%10; b/=10; h+=h4[br+'0'];
   br=b%10; b/=10; h+=h3[br+'0'];
   br=b%10; b/=10; h+=h2[br+'0'];
   br=b%10;        h+=h1[br+'0'];

   if(blz[hash[h]]==n)return hash[h];           /* BLZ gefunden, Index zurückgeben */
   iptr=hash+h+1;

      /* die BLZs sind nach Größe sortiert, unbelegte Felder zeigen auf
       * blz[lut2_cnt_hs]. Dieser Wert ist mit MAX_INT belegt. Falls also die
       * BLZ, die einem Hashwert zugeordnet wird, größer als n ist, gibt es die
       * gesuchte Zahl im BLZ-Array nicht.
       */
   if(blz[*iptr]>n)return INVALID_BLZ;
   if(blz[*iptr]==n)return *iptr;
   if(blz[*++iptr]>n)return INVALID_BLZ;
   if(blz[*iptr]==n)return *iptr;

      /* bis hierhin dürften die meisten BLZs gefunden sein, der Rest in einer Schleife */
   while(1){
      if(blz[*++iptr]>n)return INVALID_BLZ;
      if(blz[*iptr]==n)return *iptr;
   }
}

/* Funktionen, um einzelne Felder der LUT-Datei zu extrahieren +§§§1 */
/* ###########################################################################
 * # Die folgenden Funktionen extrahieren einzelne Felder aus der LUT-Datei  #
 * # und geben sie als String oder Zahl (je nach Typ) direkt zurück. Für den #
 * # Rückgabewert kann ein Integerpointer übergeben werden; falls diese      #
 * # Variable gesetzt ist, wird der Rückgabewert in die Variable geschrieben,#
 * # falls für die Variable NULL übergeben wird, wird er ignoriert.          #
 * # Die Funktionen enthalten noch einen Paramer zweigstelle, mit dem die    #
 * # Daten der Filialen bestimmt werden. Die Hauptstelle erhält man immer    #
 * # mit zweigstelle 0; falls der Index einer Filiale zu groß ist, wird ein  #
 * # Leerstring bzw. 0 zurückgegeben und retval auf den Wert                 #
 * # LUT2_INDEX_OUT_OF_RANGE gesetzt. Die Anzahl der Filialen läßt sich mit  #
 * # der Funktion lut_filialen() ermitteln.                                  #
 * #                                                                         #
 * # Parameter der folgenden Funktionen:                                     #
 * #                                                                         #
 * # b:            Bankleitzahl                                              #
 * #               Die alten Routinen erhalten die BLZ als char * Pointer;   #
 * #               es gibt noch einen Satz neuer Funktionen (mit dem Suffix  #
 * #               _i), bei denen die BLZ ein Integerwert ist (das wurde     #
 * #               für die Suchroutinen benötigt)                            #
 * #                                                                         #
 * # zweigstelle:  Index der Nebenstelle; 0 für Hauptstelle                  #
 * #                                                                         #
 * # retval:       Pointervariable, in die der Rückgabewert geschrieben      #
 * #               wird. Falls für retval NULL übergeben wird, wird der      #
 * #               Rückgabewert verworfen.                                   #
 * #                                                                         #
 * # Copyright (C) 2007 Michael Plugge <m.plugge@hs-mannheim.de>             #
 * ###########################################################################
 */

/* Funktion lut_filialen() +§§§2 */
/* ###########################################################################
 * # lut_filialen(): Anzahl der Filialen zu einer gegebenen Bankleitzahl     #
 * # bestimmen.                                                              #
 * #                                                                         #
 * # Copyright (C) 2007 Michael Plugge <m.plugge@hs-mannheim.de>             #
 * ###########################################################################
 */

DLL_EXPORT int lut_filialen(char *b,int *retval)
{
   int idx;

   if(!filialen)INVALID_I(LUT2_FILIALEN_NOT_INITIALIZED);
   if((idx=lut_index(b))<0)INVALID_I(idx);
   if(retval)*retval=OK;
   return filialen[idx];
}

DLL_EXPORT int lut_filialen_i(int b,int *retval)
{
   int idx;

   if(!filialen)INVALID_I(LUT2_FILIALEN_NOT_INITIALIZED);
   if((idx=lut_index_i(b))<0)INVALID_I(idx);
   if(retval)*retval=OK;
   return filialen[idx];
}

/* Funktion lut_name() +§§§2 */
/* ###########################################################################
 * # lut_name(): Banknamen (lange Form) bestimmen                            #
 * #                                                                         #
 * # Copyright (C) 2007 Michael Plugge <m.plugge@hs-mannheim.de>             #
 * ###########################################################################
 */

DLL_EXPORT char *lut_name(char *b,int zweigstelle,int *retval)
{
   int idx;

   if(!name)INVALID_C(LUT2_NAME_NOT_INITIALIZED);
   if((idx=lut_index(b))<0)INVALID_C(idx);
   CHECK_OFFSET_S;
   return name[startidx[idx]+zweigstelle];
}

DLL_EXPORT char *lut_name_i(int b,int zweigstelle,int *retval)
{
   int idx;

   if(!name)INVALID_C(LUT2_NAME_NOT_INITIALIZED);
   if((idx=lut_index_i(b))<0)INVALID_C(idx);
   CHECK_OFFSET_S;
   return name[startidx[idx]+zweigstelle];
}

/* Funktion lut_name_kurz() +§§§2 */
/* ###########################################################################
 * # lut_name_kurz(): Kurzbezeichnung mit Ort einer Bank bestimmen           #
 * #                                                                         #
 * # Kurzbezeichnung und Ort sollen für die Empfängerangaben auf Rechnungen  #
 * # und Formularen angegeben werden. Hierdurch wird eine eindeutige Zu-     #
 * # ordnung der eingereichten Zahlungsaufträge ermöglicht. Auf Grund der    #
 * # Regelungen in den Richtlinien beziehungsweise Zahlungsverkehrs-Abkommen #
 * # der deutschen Kreditwirtschaft ist die Länge der Angaben für die        #
 * # Bezeichnung des Kreditinstituts begrenzt.                               #
 * #                                                                         #
 * # Copyright (C) 2007 Michael Plugge <m.plugge@hs-mannheim.de>             #
 * ###########################################################################
 */

DLL_EXPORT char *lut_name_kurz(char *b,int zweigstelle,int *retval)
{
   int idx;

   if(!name_kurz)INVALID_C(LUT2_NAME_KURZ_NOT_INITIALIZED);
   if((idx=lut_index(b))<0)INVALID_C(idx);
   CHECK_OFFSET_S;
   return name_kurz[startidx[idx]+zweigstelle];
}

DLL_EXPORT char *lut_name_kurz_i(int b,int zweigstelle,int *retval)
{
   int idx;

   if(!name_kurz)INVALID_C(LUT2_NAME_KURZ_NOT_INITIALIZED);
   if((idx=lut_index_i(b))<0)INVALID_C(idx);
   CHECK_OFFSET_S;
   return name_kurz[startidx[idx]+zweigstelle];
}

/* Funktion lut_plz() +§§§2 */
/* ###########################################################################
 * # lut_plz(): Postleitzahl bestimmen                                       #
 * #                                                                         #
 * # Copyright (C) 2007 Michael Plugge <m.plugge@hs-mannheim.de>             #
 * ###########################################################################
 */

DLL_EXPORT int lut_plz(char *b,int zweigstelle,int *retval)
{
   int idx;

   if(!plz)INVALID_I(LUT2_PLZ_NOT_INITIALIZED);
   if((idx=lut_index(b))<0)INVALID_I(idx);
   CHECK_OFFSET_I;
   return plz[startidx[idx]+zweigstelle];
}

DLL_EXPORT int lut_plz_i(int b,int zweigstelle,int *retval)
{
   int idx;

   if(!plz)INVALID_I(LUT2_PLZ_NOT_INITIALIZED);
   if((idx=lut_index_i(b))<0)INVALID_I(idx);
   CHECK_OFFSET_I;
   return plz[startidx[idx]+zweigstelle];
}

/* Funktion lut_ort() +§§§2 */
/* ###########################################################################
 * # lut_ort(): Sitz einer Bank bestimmen                                    #
 * #                                                                         #
 * # Copyright (C) 2007 Michael Plugge <m.plugge@hs-mannheim.de>             #
 * ###########################################################################
 */

DLL_EXPORT char *lut_ort(char *b,int zweigstelle,int *retval)
{
   int idx;

   if(!ort)INVALID_C(LUT2_ORT_NOT_INITIALIZED);
   if((idx=lut_index(b))<0)INVALID_C(idx);
   CHECK_OFFSET_S;
   return ort[startidx[idx]+zweigstelle];
}

DLL_EXPORT char *lut_ort_i(int b,int zweigstelle,int *retval)
{
   int idx;

   if(!ort)INVALID_C(LUT2_ORT_NOT_INITIALIZED);
   if((idx=lut_index_i(b))<0)INVALID_C(idx);
   CHECK_OFFSET_S;
   return ort[startidx[idx]+zweigstelle];
}

/* Funktion lut_pan() +§§§2 */
/* ###########################################################################
 * # lut_pan(): PAN-Nummer bestimmen                                         #
 * #                                                                         #
 * # Für den internationalen Kartenzahlungsverkehr mittels Bankkunden-       #
 * # karten haben die Spitzenverbände des Kreditgewerbes und die Deutsche    #
 * # Bundesbank eine gesonderte Institutsnummerierung festgelegt; danach     #
 * # erhält das kartenausgebende Kreditinstitut eine fünfstellige Instituts- #
 * # nummer für PAN (= Primary Account Number). Diese setzt sich zusammen    #
 * # aus der Institutsgruppennummer (grundsätzlich = vierte Stelle der       #
 * # Bankleitzahl) und einer nachfolgenden vierstelligen, von den einzelnen  #
 * # Institutionen frei gewählten Nummer. Abweichend hiervon ist den         #
 * # Mitgliedsinstituten des Bundesverbandes deutscher Banken e.V. sowie     #
 * # den Stellen der Deutschen Bundesbank stets die Institutsgruppennummer   #
 * # 2 zugewiesen worden.                                                    #
 * #                                                                         #
 * # Copyright (C) 2007 Michael Plugge <m.plugge@hs-mannheim.de>             #
 * ###########################################################################
 */

DLL_EXPORT int lut_pan(char *b,int zweigstelle,int *retval)
{
   int idx;

   if(!pan)INVALID_I(LUT2_PAN_NOT_INITIALIZED);
   if((idx=lut_index(b))<0)INVALID_I(idx);
   CHECK_OFFSET_I;
   return pan[startidx[idx]+zweigstelle];
}

DLL_EXPORT int lut_pan_i(int b,int zweigstelle,int *retval)
{
   int idx;

   if(!pan)INVALID_I(LUT2_PAN_NOT_INITIALIZED);
   if((idx=lut_index_i(b))<0)INVALID_I(idx);
   CHECK_OFFSET_I;
   return pan[startidx[idx]+zweigstelle];
}

/* Funktion lut_bic() +§§§2 */
/* ###########################################################################
 * # lut_bic(): BIC (Bank Identifier Code) einer Bank bestimmen.             #
 * #                                                                         #
 * # Der Bank Identifier Code (BIC) besteht aus acht oder elf                #
 * # zusammenhängenden Stellen und setzt sich aus den Komponenten BANKCODE   #
 * # (4 Stellen), LÄNDERCODE (2 Stellen), ORTSCODE (2 Stellen) sowie ggf.    #
 * # einem FILIALCODE (3 Stellen) zusammen.                                  #
 * #                                                                         #
 * # Jedes Kreditinstitut führt grundsätzlich einen BIC je Bankleitzahl und  #
 * # teilt diesen der Deutschen Bundesbank mit. Ausnahmen hiervon können auf #
 * # Antrag für Bankleitzahlen zugelassen werden, die im BIC-gestützten      #
 * # Zahlungsverkehr (grenzüberschreitender Zahlungsverkehr und inländischer #
 * # Individualzahlungsverkehr) nicht verwendet werden.                      #
 * #                                                                         #
 * # Copyright (C) 2007 Michael Plugge <m.plugge@hs-mannheim.de>             #
 * ###########################################################################
 */

DLL_EXPORT char *lut_bic(char *b,int zweigstelle,int *retval)
{
   int idx;

   if(!bic)INVALID_C(LUT2_BIC_NOT_INITIALIZED);
   if((idx=lut_index(b))<0)INVALID_C(idx);
   CHECK_OFFSET_S;
   return bic[startidx[idx]+zweigstelle];
}

DLL_EXPORT char *lut_bic_i(int b,int zweigstelle,int *retval)
{
   int idx;

   if(!bic)INVALID_C(LUT2_BIC_NOT_INITIALIZED);
   if((idx=lut_index_i(b))<0)INVALID_C(idx);
   CHECK_OFFSET_S;
   return bic[startidx[idx]+zweigstelle];
}

/* Funktion lut_nr() +§§§2 */
/* ###########################################################################
 * # lut_nr(): Nummer des Datensatzes in der BLZ-Datei                       #
 * #                                                                         #
 * # Bei jeder Neuanlage eines Datensatzes wird von der Deutschen Bundesbank #
 * # automatisiert eine eindeutige Nummer vergeben. Eine einmal verwendete   #
 * # Nummer wird nicht noch einmal vergeben.                                 #
 * #                                                                         #
 * # Copyright (C) 2009 Michael Plugge <m.plugge@hs-mannheim.de>             #
 * ###########################################################################
 */

DLL_EXPORT int lut_nr(char *b,int zweigstelle,int *retval)
{
   int idx;

   if(!bank_nr)INVALID_I(LUT2_NR_NOT_INITIALIZED);
   if((idx=lut_index(b))<0)INVALID_I(idx);
   CHECK_OFFSET_I;
   return bank_nr[startidx[idx]+zweigstelle];
}

DLL_EXPORT int lut_nr_i(int b,int zweigstelle,int *retval)
{
   int idx;

   if(!bank_nr)INVALID_I(LUT2_NR_NOT_INITIALIZED);
   if((idx=lut_index_i(b))<0)INVALID_I(idx);
   CHECK_OFFSET_I;
   return bank_nr[startidx[idx]+zweigstelle];
}

/* Funktion lut_pz() +§§§2 */
/* ###########################################################################
 * # lut_pz(): Prüfzifferverfahren für eine Bankleitzahl. Das Verfahren wird #
 * # numerisch zurückgegeben, also z.B. 108 für die Methode A8.              #
 * #                                                                         #
 * # Copyright (C) 2007 Michael Plugge <m.plugge@hs-mannheim.de>             #
 * ###########################################################################
 */

DLL_EXPORT int lut_pz(char *b,int zweigstelle,int *retval)
{
   int idx;

   if(!pz_methoden)INVALID_I(LUT2_PZ_NOT_INITIALIZED);
   if((idx=lut_index(b))<0)INVALID_I(idx);
   CHECK_OFFSET_I;
   return pz_methoden[idx];
}

DLL_EXPORT int lut_pz_i(int b,int zweigstelle,int *retval)
{
   int idx;

   if(!pz_methoden)INVALID_I(LUT2_PZ_NOT_INITIALIZED);
   if((idx=lut_index_i(b))<0)INVALID_I(idx);
   CHECK_OFFSET_I;
   return pz_methoden[idx];
}

/* Funktion lut_aenderung() +§§§2 */
/* ###########################################################################
 * # lut_aenderung(): Änderungskennzeichen einer Bank betimmen (A Addition,  #
 * # M Modified, U Unchanged, D Deletion). Gelöschte Datensätze werden mit   #
 * # dem Kennzeichen 'D' gekennzeichnet und sind - als Hinweis - letztmalig  #
 * # in der Bankleitzahlendatei enthalten. Diese Datensätze sind ab dem      #
 * # Gültigkeitstermin der Bankleitzahlendatei im Zahlungsverkehr nicht mehr #
 * # zu verwenden.                                                           #
 * #                                                                         #
 * # Copyright (C) 2007 Michael Plugge <m.plugge@hs-mannheim.de>             #
 * ###########################################################################
 */

DLL_EXPORT int lut_aenderung(char *b,int zweigstelle,int *retval)
{
   int idx;

   if(!aenderung)INVALID_I(LUT2_AENDERUNG_NOT_INITIALIZED);
   if((idx=lut_index(b))<0)INVALID_I(idx);
   CHECK_OFFSET_I;
   return aenderung[startidx[idx]+zweigstelle];
}

DLL_EXPORT int lut_aenderung_i(int b,int zweigstelle,int *retval)
{
   int idx;

   if(!aenderung)INVALID_I(LUT2_AENDERUNG_NOT_INITIALIZED);
   if((idx=lut_index_i(b))<0)INVALID_I(idx);
   CHECK_OFFSET_I;
   return aenderung[startidx[idx]+zweigstelle];
}

/* Funktion lut_loeschung() +§§§2 */
/* ###########################################################################
 * # lut_loeschung(): Hinweis auf eine beabsichtigte Bankleitzahllöschung    #
 * #                                                                         #
 * # Zur frühzeitigen Information der Teilnehmer am Zahlungsverkehr und      #
 * # zur Beschleunigung der Umstellung der Bankverbindung kann ein Kredit-   #
 * # institut, das die Löschung einer Bankleitzahl mit dem Merkmal 1 im      #
 * # Feld 2 (Hauptstelle) beabsichtigt, die Löschung ankündigen. Die         #
 * # Ankündigung kann erfolgen, sobald das Kreditinstitut seine Kunden       #
 * # über die geänderte Kontoverbindung informiert hat. Es wird empfohlen,   #
 * # diese Ankündigung mindestens eine Änderungsperiode vor der              #
 * # eigentlichen Löschung anzuzeigen.                                       #
 * #                                                                         #
 * # Das Feld enthält das Merkmal 0 (keine Angabe) oder 1 (BLZ im Feld 1     #
 * # ist zur Löschung vorgesehen).                                           #
 * #                                                                         #
 * # Copyright (C) 2007 Michael Plugge <m.plugge@hs-mannheim.de>             #
 * ###########################################################################
 */

DLL_EXPORT int lut_loeschung(char *b,int zweigstelle,int *retval)
{
   int idx;

   if(!loeschung)INVALID_I(LUT2_LOESCHUNG_NOT_INITIALIZED);
   if((idx=lut_index(b))<0)INVALID_I(idx);
   CHECK_OFFSET_I;
   return loeschung[startidx[idx]+zweigstelle];
}

DLL_EXPORT int lut_loeschung_i(int b,int zweigstelle,int *retval)
{
   int idx;

   if(!loeschung)INVALID_I(LUT2_LOESCHUNG_NOT_INITIALIZED);
   if((idx=lut_index_i(b))<0)INVALID_I(idx);
   CHECK_OFFSET_I;
   return loeschung[startidx[idx]+zweigstelle];
}

/* Funktion lut_nachfolge_blz() +§§§2 */
/* ###########################################################################
 * # lut_nachfolge_blz(): entweder 0 (Bankleitzahl ist nicht zur Löschung    #
 * # vorgesehen, bzw. das Institut hat keine Nachfolge-BLZ veröffentlicht)   #
 * # oder eine Bankleitzahl. Eine Bankleitzahl kann nur für Hauptstellen an- #
 * # gegeben werden werden, wenn die Bankleitzahl zur Löschung angekündigt   #
 * # wurde (lut_loeschung()==1) oder die Bankleitzahl zum aktuellen Gültig-  #
 * # keitstermin gelöscht wird (lut_aenderung()=='D').                       #
 * #                                                                         #
 * # Copyright (C) 2007 Michael Plugge <m.plugge@hs-mannheim.de>             #
 * ###########################################################################
 */

DLL_EXPORT int lut_nachfolge_blz(char *b,int zweigstelle,int *retval)
{
   int idx;

   if(!nachfolge_blz)INVALID_I(LUT2_NACHFOLGE_BLZ_NOT_INITIALIZED);
   if((idx=lut_index(b))<0)INVALID_I(idx);
   CHECK_OFFSET_I;
   return nachfolge_blz[startidx[idx]+zweigstelle];
}

DLL_EXPORT int lut_nachfolge_blz_i(int b,int zweigstelle,int *retval)
{
   int idx;

   if(!nachfolge_blz)INVALID_I(LUT2_NACHFOLGE_BLZ_NOT_INITIALIZED);
   if((idx=lut_index_i(b))<0)INVALID_I(idx);
   CHECK_OFFSET_I;
   return nachfolge_blz[startidx[idx]+zweigstelle];
}

/* Funktion lut_multiple() +§§§2 */
/* ###########################################################################
 * # lut_multiple(): Universalfunktion, um zu einer gegebenen Bankleitzahl   #
 * # mehrere Werte der LUT-Datei zu bestimmen. Die gewünschten Variablen     #
 * # werden der Funktion als Referenz übergeben; die Funktion schreibt dann  #
 * # die Anfangsadresse im zugehörigen internen Array in die übergebenen     #
 * # Variablen (müssen ebenfalls Arraypointer sein!), und übergibt in der    #
 * # Variablen cnt noch die Anzahl der Zweigstellen.                         #
 * #                                                                         #
 * # Falls für die Bankleitzahl NULL oder ein Leerstring angegeben wird,     #
 * # werden die Anfangsadressen der interenen Arrays und als Anzahl (cnt)    #
 * # die Anzahl der Hauptstellen zurückgegeben. Die Anzahl aller Werte       #
 * # findet sich in der Variablen cnt_all (vorletzte Variable); das Array    #
 * # mit den Indizes der Hauptstellen wird in der letzten Variable zurück-   #
 * # gegeben (start_idx).                                                    #
 * #                                                                         #
 * # Copyright (C) 2007 Michael Plugge <m.plugge@hs-mannheim.de>             #
 * ###########################################################################
 */

#define SET_NULL if(cnt)*cnt=0; if(p_blz)*p_blz=NULL; if(p_name)*p_name=NULL; \
      if(p_name_kurz)*p_name_kurz=NULL; if(p_plz)*p_plz=NULL; if(p_ort)*p_ort=NULL; \
      if(p_pan)*p_pan=NULL; if(p_bic)*p_bic=NULL; if(p_pz)*p_pz=-1; if(p_nr)*p_nr=NULL; \
      if(p_aenderung)*p_aenderung=NULL; if(p_loeschung)*p_loeschung=NULL; \
      if(p_nachfolge_blz)*p_nachfolge_blz=NULL; if(id)*id=0; if(cnt_all)*cnt_all=0; \
      if(start_idx)*start_idx=NULL

DLL_EXPORT int lut_multiple(char *b,int *cnt,int **p_blz,char  ***p_name,char ***p_name_kurz,
      int **p_plz,char ***p_ort,int **p_pan,char ***p_bic,int *p_pz,int **p_nr,
      char **p_aenderung,char **p_loeschung,int **p_nachfolge_blz,int *id,
      int *cnt_all,int **start_idx)
{
   int idx;

   if(init_status<7){
      SET_NULL;
      return LUT2_NOT_INITIALIZED;   /* Bankleitzahlen noch nicht initialisiert */
   }
   if(cnt_all)*cnt_all=lut2_cnt;
   if(start_idx)*start_idx=startidx;

        /* falls für die BLZ NULL oder ein Leerstring übergeben wurde, Anfangsadressen
         * und Gesamtgröße der Arrays zurückliefern.
         */
   if(!b || !*b){
      idx=0;
      if(cnt)*cnt=lut2_cnt_hs;
   }
   else{
      if((idx=lut_index(b))<0){  /* ungültige BLZ */
         SET_NULL;
         return idx;
      }
      if(cnt){
         if(!filialen)  /* nur Hauptstellen in der Datei */
            *cnt=1;
         else
            *cnt=filialen[idx];
      }
   }
   return lut_multiple_int(idx,cnt,p_blz,p_name,p_name_kurz,p_plz,p_ort,p_pan,p_bic,p_pz,
         p_nr,p_aenderung,p_loeschung,p_nachfolge_blz,id,cnt_all,start_idx);
}

DLL_EXPORT int lut_multiple_i(int b,int *cnt,int **p_blz,char  ***p_name,char ***p_name_kurz,
      int **p_plz,char ***p_ort,int **p_pan,char ***p_bic,int *p_pz,int **p_nr,
      char **p_aenderung,char **p_loeschung,int **p_nachfolge_blz,int *id,
      int *cnt_all,int **start_idx)
{
   int idx;

   if(init_status<7){
      SET_NULL;
      return LUT2_NOT_INITIALIZED;   /* Bankleitzahlen noch nicht initialisiert */
   }
   if(cnt_all)*cnt_all=lut2_cnt;
   if(start_idx)*start_idx=startidx;

        /* falls für die BLZ NULL oder ein Leerstring übergeben wurde, Anfangsadressen
         * und Gesamtgröße der Arrays zurückliefern.
         */
   if(!b){
      idx=0;
      if(cnt)*cnt=lut2_cnt_hs;
   }
   else{
      if((idx=lut_index_i(b))<0){  /* ungültige BLZ */
         SET_NULL;
         return idx;
      }
      if(cnt){
         if(!filialen)  /* nur Hauptstellen in der Datei */
            *cnt=1;
         else
            *cnt=filialen[idx];
      }
   }
   return lut_multiple_int(idx,cnt,p_blz,p_name,p_name_kurz,p_plz,p_ort,p_pan,p_bic,p_pz,
         p_nr,p_aenderung,p_loeschung,p_nachfolge_blz,id,cnt_all,start_idx);
}

static int lut_multiple_int(int idx,int *cnt,int **p_blz,char  ***p_name,char ***p_name_kurz,
      int **p_plz,char ***p_ort,int **p_pan,char ***p_bic,int *p_pz,int **p_nr,
      char **p_aenderung,char **p_loeschung,int **p_nachfolge_blz,int *id,
      int *cnt_all,int **start_idx)
{
   int start,retval=OK;

   start=startidx[idx];
   if(id)*id=idx;

   if(p_blz){
      if(!blz){
         *p_blz=leer_zahl;
         retval=LUT2_PARTIAL_OK;
      }
      else
         *p_blz=blz+start;
   }

   if(p_name){
      if(!name){
         *p_name=leer_string;
         retval=LUT2_PARTIAL_OK;
      }
      else
         *p_name=name+start;
   }

   if(p_name_kurz){
      if(!name_kurz){
         *p_name_kurz=leer_string;
         retval=LUT2_PARTIAL_OK;
      }
      else
         *p_name_kurz=name_kurz+start;
   }

   if(p_plz){
      if(!plz){
         *p_plz=leer_zahl;
         retval=LUT2_PARTIAL_OK;
      }
      else
         *p_plz=plz+start;
   }

   if(p_ort){
      if(!ort){
         *p_ort=leer_string;
         retval=LUT2_PARTIAL_OK;
      }
      else
         *p_ort=ort+start;
   }

   if(p_pan){
      if(!pan){
         *p_pan=leer_zahl;
         retval=LUT2_PARTIAL_OK;
      }
      else
         *p_pan=pan+start;
   }

   if(p_bic){
      if(!bic){
         *p_bic=leer_string;
         retval=LUT2_PARTIAL_OK;
      }
      else
         *p_bic=bic+start;
   }

   if(p_pz){
      if(!pz_methoden){
         *p_pz=-1;
         retval=LUT2_PARTIAL_OK;
      }
      else
         *p_pz=pz_methoden[idx];    /* PZ-Methoden werden nicht für die Filialen gespeichert, daher idx */
   }

   if(p_nr){
      if(!bank_nr){
         *p_nr=leer_zahl;
         retval=LUT2_PARTIAL_OK;
      }
      else
         *p_nr=bank_nr+start;
   }

   if(p_aenderung){
      if(!aenderung){
         *p_aenderung=leer_char;
         retval=LUT2_PARTIAL_OK;
      }
      else
         *p_aenderung=aenderung+start;
   }

   if(p_loeschung){
      if(!loeschung){
         *p_loeschung=leer_char;
         retval=LUT2_PARTIAL_OK;
      }
      else
         *p_loeschung=loeschung+start;
   }

   if(p_nachfolge_blz){
      if(!nachfolge_blz){
         *p_nachfolge_blz=leer_zahl;
         retval=LUT2_PARTIAL_OK;
      }
      else
         *p_nachfolge_blz=nachfolge_blz+start;
   }
   return retval;
}

/* Funktion lut_cleanup() +§§§1 */
/* ###########################################################################
 * # lut_cleanup(): Aufräuarbeiten                                           #
 * # Die Funktion lut_cleanup() gibt allen belegten Speicher frei und setzt  #
 * # die entsprechenden Variablen auf NULL.                                  #
 * #                                                                         #
 * # Copyright (C) 2007 Michael Plugge <m.plugge@hs-mannheim.de>             #
 * ###########################################################################
 */

DLL_EXPORT int lut_cleanup(void)
{
   int i;

   INITIALIZE_WAIT;     /* zunächst testen, ob noch eine andere Initialisierung läuft (z.B. in einem anderen Thread) */
   init_in_progress=1;  /* Lockflag für Tests und Initialierung setzen */
   init_status|=16;     /* init_status wird bei der Prüfung getestet */
   *lut_id=0;
   lut_id_status=0;
   lut_init_level=-1;
   if(init_status&8)INITIALIZE_WAIT;

      /* falls kto_check_init() den Notausstieg mit INIT_FATAL_ERROR gemacht
       * hatte, wurde init_in_progress auf 0 gesetzt; jetzt wieder setzen.
       */
   init_in_progress=1;
   if(blz_f && startidx[lut2_cnt_hs-1]==lut2_cnt_hs-1)
      blz_f=NULL; /* kein eigenes Array */
   else{
      FREE(blz_f);
   }
   FREE(zweigstelle_f);
   FREE(zweigstelle_f1);
   FREE(sort_bic);
   FREE(sort_name);
   FREE(sort_name_kurz);
   FREE(sort_ort);
   FREE(sort_blz);
   FREE(sort_pz_methoden);
   FREE(sort_plz);
   FREE(name);
   FREE(name_data);
   FREE(name_kurz);
   FREE(name_kurz_data);
   FREE(name_name_kurz_data);
   FREE(ort);
   FREE(ort_data);
   FREE(bic);
   FREE(bic_buffer);
   FREE(aenderung);
   FREE(loeschung);
   FREE(blz);
   FREE(startidx);
   FREE(plz);
   FREE(filialen);
   FREE(pan);
   FREE(pz_methoden);
   FREE(bank_nr);
   FREE(nachfolge_blz);
   FREE(current_info);
   FREE(own_buffer);
   FREE(hash);
   for(i=0;i<400;i++)lut2_block_status[i]=0;

   if(init_status&8){

         /* bei init_status&8 ist wohl eine Initialisierung dazwischengekommen (sollte
          * eigentlich nicht passieren); daher nur eine Fehlermeldung zurückgeben.
          */
      usleep(50000); /* etwas abwarten */
      lut_cleanup(); /* neuer Versuch, aufzuräumen */
      return INIT_FATAL_ERROR;
   }
#if CHECK_MALLOC
   xfree();
#endif
   init_status&=1;
   init_in_progress=0;
   return OK;
}

/* Funktion generate_lut() +§§§1 */
/* ###########################################################################
 * # Die Funktion generate_lut() generiert aus der BLZ-Datei der deutschen   #
 * # Bundesbank (knapp 4 MB) eine kleine Datei (ca. 14 KB), in der nur die   #
 * # Bankleitzahlen und Prüfziffermethoden gespeichert sind. Um die Datei    #
 * # klein zu halten, werden normalerweise nur Differenzen zur Vorgänger-BLZ #
 * # gespeichert (meist mit 1 oder 2 Byte); die Prüfziffermethode wird in    #
 * # einem Byte kodiert. Diese kleine Datei läßt sich dann natürlich viel    #
 * # schneller einlesen als die große Bundesbank-Datei.                      #
 * #                                                                         #
 * # Ab Version 3 wird für die Funktionalität die Routine generate_lut2()    #
 * # (mit Defaultwerten für Felder und Slots) benutzt.                       #
 * #                                                                         #
 * # Bugs: es wird für eine BLZ nur eine Prüfziffermethode unterstützt.      #
 * #      (nach Bankfusionen finden sich für eine Bank manchmal zwei         #
 * #      Prüfziffermethoden; das Problem wird mit dem neuen Dateiformat     #
 * #      noch einmal angegangen.                                            #
 * #                                                                         #
 * # Copyright (C) 2002-2005 Michael Plugge <m.plugge@hs-mannheim.de>        #
 * ###########################################################################
 */

DLL_EXPORT int generate_lut_t(char *inputname,char *outputname,char *user_info,int lut_version,KTO_CHK_CTX *ctx)
{
   return generate_lut2(inputname,outputname,user_info,NULL,NULL,0,lut_version,1);
}

DLL_EXPORT int generate_lut(char *inputname,char *outputname,char *user_info,int lut_version)
{
   return generate_lut2(inputname,outputname,user_info,NULL,NULL,0,lut_version,1);
}

/* Funktion read_lut() +§§§1 */
/* ###########################################################################
 * # Die Funktion read_lut() liest die Lookup-Tabelle mit Bankleitzahlen und #
 * # Prüfziffermethoden im alten Format (1.0/1.1) ein und führt einige       #
 * # Konsistenz-Tests durch.                                                 #
 * #                                                                         #
 * # Bugs: für eine BLZ wird nur eine Prüfziffermethode unterstützt (s.o.).  #
 * #                                                                         #
 * # Copyright (C) 2002-2005 Michael Plugge <m.plugge@hs-mannheim.de>        #
 * ###########################################################################
 */

static int read_lut(char *filename,int *cnt_blz)
{
   unsigned char *inbuffer,*uptr;
   int b,h,k,i,j,prev,cnt,lut_version;
   UINT4 adler1,adler2;
   struct stat s_buf;
   int in;

   if(cnt_blz)*cnt_blz=0;
   if(!(init_status&1))init_atoi_table();
   if(stat(filename,&s_buf)==-1)return NO_LUT_FILE;
   if(!(inbuffer=calloc(s_buf.st_size+128,1)))return ERROR_MALLOC;
   if((in=open(filename,O_RDONLY|O_BINARY))<0)return NO_LUT_FILE;
   if(!(cnt=read(in,inbuffer,s_buf.st_size)))return FILE_READ_ERROR;
   close(in);
   lut_version= -1;
   if(!strncmp((char *)inbuffer,"BLZ Lookup Table/Format 1.0\n",28))
      lut_version=1;
   if(!strncmp((char *)inbuffer,"BLZ Lookup Table/Format 1.1\n",28))
      lut_version=2;
   if(lut_version==-1)return INVALID_LUT_FILE;
   for(uptr=inbuffer,i=cnt;*uptr++!='\n';i--);  /* Signatur */
   if(lut_version==2){  /* Info-Zeile überspringen */
      for(i--,j=0;*uptr++!='\n';i--);
      if(*(uptr-2)=='\\')for(i--;*uptr++!='\n';i--); /* user_info */
   }
   i-=9;
   READ_LONG(cnt);
   READ_LONG(adler1);
   adler2=adler32a(0,(char *)uptr,i)^cnt;
   if(adler1!=adler2)return LUT_CRC_ERROR;
   if(cnt>(i-8)/2)return INVALID_LUT_FILE;

      /* zunächst u.U. Speicher freigeben, damit keine Lecks entstehen */
   FREE(blz);
   FREE(startidx);
   FREE(hash);
   FREE(pz_methoden);
   if(!(blz=calloc(j=cnt+100,sizeof(int))) || !(startidx=calloc(j,sizeof(int)))
         || !(hash=calloc(HASH_BUFFER_SIZE,sizeof(short))) || !(pz_methoden=calloc(j,sizeof(int)))){
      lut_cleanup();
      return ERROR_MALLOC;
   }

   for(j=prev=i=0;i<cnt;uptr++){
      if(*uptr<251){   /* 1 Byte Differenz, positiv */
         blz[i]=prev+*uptr;
         prev=blz[i];
         pz_methoden[i++]=*++uptr;
      }
      else switch(*uptr){
         case 251:   /* 2 Byte Differenz, negativ */
            uptr++;
            blz[i]= prev-(*uptr+(*(uptr+1)<<8));
            uptr+=2;
            prev=blz[i];
            pz_methoden[i++]=*uptr;
            break;
         case 252:   /* 1 Byte Differenz, negativ */
            uptr++;
            blz[i]= prev-*uptr;
            prev=blz[i];
            pz_methoden[i++]=*++uptr;
            break;
         case 253:   /* 4 Byte Wert der BLZ komplett, nicht Differenz */
            uptr++;
            blz[i]= *uptr+(*(uptr+1)<<8)+(*(uptr+2)<<16)+(*(uptr+3)<<24);
            uptr+=4;
            prev=blz[i];
            pz_methoden[i++]=*uptr;
            break;
         case 254:   /* 2 Byte Differenz, positiv */
            uptr++;
            blz[i]=prev+*uptr+(*(uptr+1)<<8);
            uptr+=2;
            prev=blz[i];
            pz_methoden[i++]=*uptr;
            break;
         case 255:   /* reserviert */
            return INVALID_LUT_FILE;
      }
   }
   blz[cnt]=BLZ_FEHLER;
   for(;i<cnt+5;i++)blz[i]=BLZ_FEHLER+i;
   for(i=0;i<HASH_BUFFER_SIZE;i++)hash[i]=cnt;
   for(i=0;i<cnt;i++){
      b=blz[i];      /* b BLZ, h Hashwert */
      k=b%10; h= hx8[k]; b/=10;
      k=b%10; h+=hx7[k]; b/=10;
      k=b%10; h+=hx6[k]; b/=10;
      k=b%10; h+=hx5[k]; b/=10;
      k=b%10; h+=hx4[k]; b/=10;
      k=b%10; h+=hx3[k]; b/=10;
      k=b%10; h+=hx2[k]; b/=10;
      k=b%10; h+=hx1[k];
      while(hash[h]!=cnt)h++;
      hash[h]=i;
   }
   FREE(inbuffer);
   if(cnt_blz)*cnt_blz=cnt;
   return OK;
}

/* Funktion init_atoi_table() +§§§1 */
/* ###########################################################################
 * # Die Funktion init_atoi_table initialisiert die Arrays b1 bis b8, die    #
 * # zur schnellen Umwandlung des Bankleitzahlstrings in eine Zahl dienen.   #
 * # Dazu werden 8 Arrays aufgebaut (für jede Stelle eines); die Umwandlung  #
 * # von String nach int läßt sich damit auf Arrayoperationen und Summierung #
 * # zurückführen, was wesentlich schneller ist, als die sonst nötigen acht  #
 * # Multiplikationen mit dem jeweiligen Stellenwert.                        #
 * #                                                                         #
 * # Copyright (C) 2007 Michael Plugge <m.plugge@hs-mannheim.de>             #
 * ###########################################################################
 */

static void init_atoi_table(void)
{
   int i,ziffer;
   unsigned long l;

   /* ungültige Ziffern; Blanks und Tabs werden ebenfalls als ungültig
    * angesehen(!), da die Stellenzuordnung sonst nicht mehr stimmt. Ausnahme:
    * Am Ende ist ein Blank erlaubt; die BLZ wird damit als abgeschlossen
    * angesehen.
    */
   for(i=0;i<256;i++){
      b0[i]=b1[i]=b2[i]=b3[i]=b4[i]=b5[i]=b6[i]=b7[i]=b8[i]=bx1[i]=bx2[i]=by1[i]=by4[i]=BLZ_FEHLER;
      leer_string[i]="";
      leer_zahl[i]=-1;
      leer_char[i]=0;
      lc[i]=i; /* für stri_cmp() */
   }
   b0[0]=by1[0]=by4[0]=0;  /* b0 wird nur für das Nullbyte am Ende der BLZ benutzt */
   b0[' ']=b0['\t']=0;     /* für b0 auch Blank bzw. Tab akzeptieren */

      /* eigentliche Ziffern belegen */
   for(i='0';i<='9';i++){
      ziffer=i-'0';
      bx1[i]=by1[i]=b1[i]=ziffer; ziffer*=10;
      bx2[i]=b2[i]=ziffer; ziffer*=10;
      b3[i]=ziffer; ziffer*=10;
      by4[i]=b4[i]=ziffer; ziffer*=10;
      b5[i]=ziffer; ziffer*=10;
      b6[i]=ziffer; ziffer*=10;
      b7[i]=ziffer; ziffer*=10;
      b8[i]=ziffer;
   }
   for(i='a';i<'z';i++){ /* Sonderfall für bx1, bx2 und by4: Buchstaben a-z => 10...36 (Prüfziffermethoden) */
      bx1[i]=(i-'a'+10);   /* bx1: a...z => 10...36 */
      by1[i]=(i-'a'+1);    /* by1: a...z => 1...26 (für Untermethoden direkt) */
      bx2[i]=bx1[i]*10;    /* bx2: a...z => 100...360 (Prüfziffermethoden 1. Stelle) */
      by4[i]=(i-'a'+1)*1000;  /* Untermethode von Prüfziffern (in der Debugversion) */
   }
   for(i='A';i<='Z';i++){ /* wie oben, nur für Großbuchstaben */
      bx1[i]=(i-'A'+10);
      by1[i]=(i-'A'+1);
      bx2[i]=bx1[i]*10;
      by4[i]=(i-'A'+1)*1000;
      lc[i]=i-'A'+'a';
   }
      /* Umlaute für Groß/Kleinschreibung (ISO-8859-1): unter die einfachen
       * Buchstaben einsortieren. Die Umlaute müssen nach unsigned int
       * konvertiert werden, da sie ansonsten u.U. mit Vorzeichen versehen
       * werden und Durcheinander anrichten.
       */
   lc[UI 'á']=lc[UI 'Á']='a';
   lc[UI 'ä']=lc[UI 'Ä']='a';
   lc[UI 'à']=lc[UI 'À']='a';
   lc[UI 'â']=lc[UI 'Â']='a';
   lc[UI 'é']=lc[UI 'É']='e';
   lc[UI 'è']=lc[UI 'È']='e';
   lc[UI 'ê']=lc[UI 'Ê']='e';
   lc[UI 'í']=lc[UI 'Í']='i';
   lc[UI 'ì']=lc[UI 'Ì']='i';
   lc[UI 'î']=lc[UI 'Î']='i';
   lc[UI 'ö']=lc[UI 'Ö']='o';
   lc[UI 'ó']=lc[UI 'Ó']='o';
   lc[UI 'ò']=lc[UI 'Ò']='o';
   lc[UI 'ô']=lc[UI 'Ô']='o';
   lc[UI 'ü']=lc[UI 'Ü']='u';
   lc[UI 'ú']=lc[UI 'Ú']='u';
   lc[UI 'ù']=lc[UI 'Ù']='u';
   lc[UI 'û']=lc[UI 'Û']='u';
   lc[UI 'ß']='s';

   for(i=0;i<=9;i++){   /* Hasharrays initialisieren */
      h1[i+'0']=hx1[i];
      h2[i+'0']=hx2[i];
      h3[i+'0']=hx3[i];
      h4[i+'0']=hx4[i];
      h5[i+'0']=hx5[i];
      h6[i+'0']=hx6[i];
      h7[i+'0']=hx7[i];
      h8[i+'0']=hx8[i];
   }
#if COMPRESS>0
   l=sizeof(ee);
   if(uncompress(ee,&l,eec,sizeof(eec))==Z_OK){
      eep=ee+1;
      eeh=ee+*ee;
   }
#endif

   for(i=0;i<255;i++){
      lut_block_name1[i]="nicht definiert";
      lut_block_name2[i]="nicht def.";
      lut2_feld_namen[i]="";
   }
   lut_block_name2[0]="leer";

   lut2_feld_namen[LUT2_BLZ]="LUT2_BLZ";
   lut2_feld_namen[LUT2_2_BLZ]="LUT2_2_BLZ";
   lut2_feld_namen[LUT2_FILIALEN]="LUT2_FILIALEN";
   lut2_feld_namen[LUT2_2_FILIALEN]="LUT2_2_FILIALEN";
   lut2_feld_namen[LUT2_NAME]="LUT2_NAME";
   lut2_feld_namen[LUT2_2_NAME]="LUT2_2_NAME";
   lut2_feld_namen[LUT2_PLZ]="LUT2_PLZ";
   lut2_feld_namen[LUT2_2_PLZ]="LUT2_2_PLZ";
   lut2_feld_namen[LUT2_ORT]="LUT2_ORT";
   lut2_feld_namen[LUT2_2_ORT]="LUT2_2_ORT";
   lut2_feld_namen[LUT2_NAME_KURZ]="LUT2_NAME_KURZ";
   lut2_feld_namen[LUT2_2_NAME_KURZ]="LUT2_2_NAME_KURZ";
   lut2_feld_namen[LUT2_PAN]="LUT2_PAN";
   lut2_feld_namen[LUT2_2_PAN]="LUT2_2_PAN";
   lut2_feld_namen[LUT2_BIC]="LUT2_BIC";
   lut2_feld_namen[LUT2_2_BIC]="LUT2_2_BIC";
   lut2_feld_namen[LUT2_PZ]="LUT2_PZ";
   lut2_feld_namen[LUT2_2_PZ]="LUT2_2_PZ";
   lut2_feld_namen[LUT2_NR]="LUT2_NR";
   lut2_feld_namen[LUT2_2_NR]="LUT2_2_NR";
   lut2_feld_namen[LUT2_AENDERUNG]="LUT2_AENDERUNG";
   lut2_feld_namen[LUT2_2_AENDERUNG]="LUT2_2_AENDERUNG";
   lut2_feld_namen[LUT2_LOESCHUNG]="LUT2_LOESCHUNG";
   lut2_feld_namen[LUT2_2_LOESCHUNG]="LUT2_2_LOESCHUNG";
   lut2_feld_namen[LUT2_NACHFOLGE_BLZ]="LUT2_NACHFOLGE_BLZ";
   lut2_feld_namen[LUT2_2_NACHFOLGE_BLZ]="LUT2_2_NACHFOLGE_BLZ";
   lut2_feld_namen[LUT2_NAME_NAME_KURZ]="LUT2_NAME_NAME_KURZ";
   lut2_feld_namen[LUT2_2_NAME_NAME_KURZ]="LUT2_2_NAME_NAME_KURZ";
   lut2_feld_namen[LUT2_INFO]="LUT2_INFO";
   lut2_feld_namen[LUT2_2_INFO]="LUT2_2_INFO";

   lut_block_name2[1]="1. BLZ";
   lut_block_name2[2]="1. Anzahl Fil.";
   lut_block_name2[3]="1. Name";
   lut_block_name2[4]="1. Plz";
   lut_block_name2[5]="1. Ort";
   lut_block_name2[6]="1. Name (kurz)";
   lut_block_name2[7]="1. PAN";
   lut_block_name2[8]="1. BIC";
   lut_block_name2[9]="1. Pruefziffer";
   lut_block_name2[10]="1. Lfd. Nr.";
   lut_block_name2[11]="1. Aenderung";
   lut_block_name2[12]="1. Loeschung";
   lut_block_name2[13]="1. NachfolgeBLZ";
   lut_block_name2[14]="1. Name, Kurzn.";
   lut_block_name2[15]="1. Infoblock";

   lut_block_name2[101]="2. BLZ";
   lut_block_name2[102]="2. Anzahl Fil.";
   lut_block_name2[103]="2. Name";
   lut_block_name2[104]="2. Plz";
   lut_block_name2[105]="2. Ort";
   lut_block_name2[106]="2. Name (kurz)";
   lut_block_name2[107]="2. PAN";
   lut_block_name2[108]="2. BIC";
   lut_block_name2[109]="2. Pruefziffer";
   lut_block_name2[110]="2. Lfd. Nr.";
   lut_block_name2[111]="2. Aenderung";
   lut_block_name2[112]="2. Loeschung";
   lut_block_name2[113]="2. NachfolgeBLZ";
   lut_block_name2[114]="2. Name, Kurzn.";
   lut_block_name2[115]="2. Infoblock";

   lut_block_name1[1]="BLZ";
   lut_block_name1[2]="FILIALEN";
   lut_block_name1[3]="NAME";
   lut_block_name1[4]="PLZ";
   lut_block_name1[5]="ORT";
   lut_block_name1[6]="NAME_KURZ";
   lut_block_name1[7]="PAN";
   lut_block_name1[8]="BIC";
   lut_block_name1[9]="PZ";
   lut_block_name1[10]="NR";
   lut_block_name1[11]="AENDERUNG";
   lut_block_name1[12]="LOESCHUNG";
   lut_block_name1[13]="NACHFOLGE_BLZ";
   lut_block_name1[14]="NAME_NAME_KURZ";
   lut_block_name1[15]="INFO";

   lut_block_name1[101]="BLZ (2)";
   lut_block_name1[102]="FILIALEN (2)";
   lut_block_name1[103]="NAME (2)";
   lut_block_name1[104]="PLZ (2)";
   lut_block_name1[105]="ORT (2)";
   lut_block_name1[106]="NAME_KURZ (2)";
   lut_block_name1[107]="PAN (2)";
   lut_block_name1[108]="BIC (2)";
   lut_block_name1[109]="PZ (2)";
   lut_block_name1[110]="NR (2)";
   lut_block_name1[111]="AENDERUNG (2)";
   lut_block_name1[112]="LOESCHUNG (2)";
   lut_block_name1[113]="NACHFOLGE_BLZ (2)";
   lut_block_name1[114]="NAME_NAME_KURZ (2)";
   lut_block_name1[115]="INFO (2)";
   init_status|=1;
}

/* Funktion kto_check_int() +§§§1
   Prolog +§§§2 */
/* ###########################################################################
 * # Die Funktion kto_check_int() ist die interne Funktion zur Überprüfung   #
 * # einer Kontonummer. Ab Version 3.0 wurde die Funktionalität umgestellt,  #
 * # indem Initialisierung und Test getrennt wurden; diese Routine führt nur #
 * # noch die Tests durch, macht keine Initialisierung mehr.                 #
 * #                                                                         #
 * # Parameter:                                                              #
 * #    x_blz:        Bankleitzahl (wird in einigen Methoden benötigt)       #
 * #    pz_methode:   Prüfziffer (numerisch)                                 #
 * #    kto:          Kontonummer                                            #
 * #                                                                         #
 * # (die beiden folgenden Parameter werden nur in der DEBUG-Version         #
 * #  unterstützt):                                                          #
 * #                                                                         #
 * #    untermethode: gewünschte Untermethode oder 0                         #
 * #    retvals:      Struktur, um Prüfziffermethode und Prüfziffer in einer #
 * #                  threadfesten Version zurückzugeben                     #
 * #                                                                         #
 * # Copyright (C) 2002-2009 Michael Plugge <m.plugge@hs-mannheim.de>        #
 * ###########################################################################
 */

#if DEBUG>0
static int kto_check_int(char *x_blz,int pz_methode,char *kto,int untermethode,RETVAL *retvals)
#else
static int kto_check_int(char *x_blz,int pz_methode,char *kto)
#endif
{
   char *ptr,*dptr,kto_alt[32],xkto[32];
   int i,p1=0,tmp,kto_len,pz1=0,ok,pz;
   int c2,d2,a5,p,konto[11];   /* Variablen für Methode 87 */

      /* Konto links mit Nullen auf 10 Stellen auffüllen,
       * und in die lokale Variable xkto umkopieren.
       */
   memset(xkto,'0',12);
   for(ptr=kto;*ptr=='0' || *ptr==' ' || *ptr=='\t';ptr++);
   for(kto_len=0;*ptr && *ptr!=' ' && *ptr!='\t';kto_len++,ptr++);
   dptr=xkto+10;
   *dptr--=0;
   for(ptr--,i=kto_len;i-->0;*dptr--= *ptr--);
   if(kto_len<1 || kto_len>10)return INVALID_KTO_LENGTH;
   kto=xkto;

/* Methoden der Prüfzifferberechnung +§§§2
   Prolog +§§§3 */
/*
 * ######################################################################
 * #               Methoden der Prüfzifferberechnung                    #
 * ######################################################################
 */

   switch(pz_methode){

/* Berechnungsmethoden 00 bis 09 +§§§3
   Berechnung nach der Methode 00 +§§§4 */
/*
 * ######################################################################
 * #               Berechnung nach der Methode 00                       #
 * ######################################################################
 * # Modulus 10, Gewichtung 2, 1, 2, 1, 2, 1, 2, 1, 2.                  #
 * # Die einzelnen Stellen der Kontonummer sind von rechts nach         #
 * # links mit den Ziffern 2, 1, 2, 1, 2 usw. zu multiplizieren.        #
 * # Die jeweiligen Produkte werden addiert, nachdem jeweils aus        #
 * # den zweistelligen Produkten die Quersumme gebildet wurde           #
 * # (z.B. Produkt 16 = Quersumme 7). Nach der Addition bleiben         #
 * # außer der Einerstelle alle anderen Stellen unberücksichtigt.       #
 * # Die Einerstelle wird von dem Wert 10 subtrahiert. Das Ergebnis     #
 * # ist die Prüfziffer. Ergibt sich nach der Subtraktion der           #
 * # Rest 10, ist die Prüfziffer 0.                                     #
 * ######################################################################
 */

      case 0:
#if DEBUG>0
         if(retvals){
            retvals->methode="00";
            retvals->pz_methode=0;
         }
#endif

            /* Alpha: etwas andere Berechnung, wesentlich schneller
             * (benötigt nur 75 statt 123 Takte, falls Berechnung wie Intel.
             * Die Intel-Methode wäre somit sogar langsamer als die alte Version
             * mit 105 Takten)
             */
#ifdef __ALPHA
         pz = ((kto[0]<'5') ? (kto[0]-'0')*2 : (kto[0]-'0')*2-9)
            +  (kto[1]-'0')
            + ((kto[2]<'5') ? (kto[2]-'0')*2 : (kto[2]-'0')*2-9)
            +  (kto[3]-'0')
            + ((kto[4]<'5') ? (kto[4]-'0')*2 : (kto[4]-'0')*2-9)
            +  (kto[5]-'0')
            + ((kto[6]<'5') ? (kto[6]-'0')*2 : (kto[6]-'0')*2-9)
            +  (kto[7]-'0')
            + ((kto[8]<'5') ? (kto[8]-'0')*2 : (kto[8]-'0')*2-9);
#else
         pz=(kto[1]-'0')+(kto[3]-'0')+(kto[5]-'0')+(kto[7]-'0');
         if(kto[0]<'5')pz+=(kto[0]-'0')*2; else pz+=(kto[0]-'0')*2-9;
         if(kto[2]<'5')pz+=(kto[2]-'0')*2; else pz+=(kto[2]-'0')*2-9;
         if(kto[4]<'5')pz+=(kto[4]-'0')*2; else pz+=(kto[4]-'0')*2-9;
         if(kto[6]<'5')pz+=(kto[6]-'0')*2; else pz+=(kto[6]-'0')*2-9;
         if(kto[8]<'5')pz+=(kto[8]-'0')*2; else pz+=(kto[8]-'0')*2-9;
#endif
         MOD_10_80;   /* pz%=10 */
         if(pz)pz=10-pz;
         CHECK_PZ10;


/*  Berechnung nach der Methode 01 +§§§4 */
/*
 * ######################################################################
 * #               Berechnung nach der Methode 01                       #
 * ######################################################################
 * # Modulus 10, Gewichtung 3, 7, 1, 3, 7, 1, 3, 7, 1.                  #
 * # Die einzelnen Stellen der Kontonummer sind von rechts nach         #
 * # links mit den Ziffern 3, 7, 1, 3, 7, 1 usw. zu multiplizieren.     #
 * # Die jeweiligen Produkte werden addiert. Nach der Addition          #
 * # bleiben außer der Einerstelle alle anderen Stellen                 #
 * # Unberücksichtigt. Die Einerstelle wird von dem Wert 10             #
 * # subtrahiert. Das Ergebnis ist die Prüfziffer. Ergibt sich nach     #
 * # der Subtraktion der Rest 10, ist die Prüfziffer 0.                 #
 * ######################################################################
 */
      case 1:
#if DEBUG>0
         if(retvals){
            retvals->methode="01";
            retvals->pz_methode=1;
         }
#endif
         pz = (kto[0]-'0')
            + (kto[1]-'0') * 7
            + (kto[2]-'0') * 3
            + (kto[3]-'0')
            + (kto[4]-'0') * 7
            + (kto[5]-'0') * 3
            + (kto[6]-'0')
            + (kto[7]-'0') * 7
            + (kto[8]-'0') * 3;

         MOD_10_160;   /* pz%=10 */
         if(pz)pz=10-pz;
         CHECK_PZ10;

/*  Berechnung nach der Methode 02 +§§§4 */
/*
 * ######################################################################
 * #               Berechnung nach der Methode 02                       #
 * ######################################################################
 * # Modulus 11, Gewichtung 2, 3, 4, 5, 6, 7, 8, 9, 2.                  #
 * # Die einzelnen Stellen der Kontonummer sind von rechts nach         #
 * # links mit den Ziffern 2, 3, 4, 5, 6, 7, 8, 9, 2 zu                 #
 * # multiplizieren. Die jeweiligen Produkte werden addiert.            #
 * # Die Summe ist durch 11 zu dividieren. Der verbleibende Rest        #
 * # wird vom Divisor (11) subtrahiert. Das Ergebnis ist die            #
 * # Prüfziffer. Verbleibt nach der Division durch 11 kein Rest,        #
 * # ist die Prüfziffer 0. Ergibt sich als Rest 1, ist die              #
 * # Prüfziffer zweistellig und kann nicht verwendet werden.            #
 * # Die Kontonummer ist dann nicht verwendbar.                         #
 * ######################################################################
 */
      case 2:
#if DEBUG>0
         if(retvals){
            retvals->methode="02";
            retvals->pz_methode=2;
         }
#endif
         pz = (kto[0]-'0') * 2
            + (kto[1]-'0') * 9
            + (kto[2]-'0') * 8
            + (kto[3]-'0') * 7
            + (kto[4]-'0') * 6
            + (kto[5]-'0') * 5
            + (kto[6]-'0') * 4
            + (kto[7]-'0') * 3
            + (kto[8]-'0') * 2;

         MOD_11_352;   /* pz%=11 */
         if(pz)pz=11-pz;
         INVALID_PZ10;
         CHECK_PZ10;

/*  Berechnung nach der Methode 03 +§§§4 */
/*
 * ######################################################################
 * #               Berechnung nach der Methode 03                       #
 * ######################################################################
 * # Modulus 10, Gewichtung 2, 1, 2, 1, 2, 1, 2, 1, 2.                  #
 * # Die Berechnung erfolgt wie bei Verfahren 01.                       #
 * ######################################################################
 */
      case 3:
#if DEBUG>0
         if(retvals){
            retvals->methode="03";
            retvals->pz_methode=3;
         }
#endif
         pz = (kto[0]-'0') * 2
            + (kto[1]-'0')
            + (kto[2]-'0') * 2
            + (kto[3]-'0')
            + (kto[4]-'0') * 2
            + (kto[5]-'0')
            + (kto[6]-'0') * 2
            + (kto[7]-'0')
            + (kto[8]-'0') * 2;

         MOD_10_80;   /* pz%=10 */
         if(pz)pz=10-pz;
         CHECK_PZ10;

/*  Berechnung nach der Methode 04 +§§§4 */
/*
 * ######################################################################
 * #               Berechnung nach der Methode 04                       #
 * ######################################################################
 * # Modulus 11, Gewichtung 2, 3, 4, 5, 6, 7, 2, 3, 4.                  #
 * # Die Berechnung erfolgt wie bei Verfahren 02.                       #
 * ######################################################################
 */
      case 4:
#if DEBUG>0
         if(retvals){
            retvals->methode="04";
            retvals->pz_methode=4;
         }
#endif
         pz = (kto[0]-'0') * 4
            + (kto[1]-'0') * 3
            + (kto[2]-'0') * 2
            + (kto[3]-'0') * 7
            + (kto[4]-'0') * 6
            + (kto[5]-'0') * 5
            + (kto[6]-'0') * 4
            + (kto[7]-'0') * 3
            + (kto[8]-'0') * 2;

         MOD_11_176;   /* pz%=11 */
         if(pz)pz=11-pz;
         INVALID_PZ10;
         CHECK_PZ10;

/*  Berechnung nach der Methode 05 +§§§4 */
/*
 * ######################################################################
 * #               Berechnung nach der Methode 05                       #
 * ######################################################################
 * # Modulus 10, Gewichtung 7, 3, 1, 7, 3, 1, 7, 3, 1.                  #
 * # Die Berechnung erfolgt wie bei Verfahren 01.                       #
 * ######################################################################
 */
      case 5:
#if DEBUG>0
         if(retvals){
            retvals->methode="05";
            retvals->pz_methode=5;
         }
#endif
         pz = (kto[0]-'0')
            + (kto[1]-'0') * 3
            + (kto[2]-'0') * 7
            + (kto[3]-'0')
            + (kto[4]-'0') * 3
            + (kto[5]-'0') * 7
            + (kto[6]-'0')
            + (kto[7]-'0') * 3
            + (kto[8]-'0') * 7;

         MOD_10_160;   /* pz%=10 */
         if(pz)pz=10-pz;
         CHECK_PZ10;

/*  Berechnung nach der Methode 06 +§§§4 */
/*
 * ######################################################################
 * #               Berechnung nach der Methode 06                       #
 * ######################################################################
 * # Modulus 11, Gewichtung 2, 3, 4, 5, 6, 7 (modifiziert)              #
 * # Die einzelnen Stellen der Kontonummer sind von rechts nach         #
 * # links mit den Ziffern 2, 3, 4, 5, 6, 7, 2, 3 ff. zu multiplizieren.#
 * # Die jeweiligen Produkte werden addiert. Die Summe ist              #
 * # durch 11 zu dividieren. Der verbleibende Rest wird vom             #
 * # Divisor (11) subtrahiert. Das Ergebnis ist die Prüfziffer.         #
 * # Ergibt sich als Rest 1, findet von dem Rechenergebnis 10           #
 * # nur die Einerstelle (0) als Prüfziffer Verwendung. Verbleibt       #
 * # nach der Division durch 11 kein Rest, dann ist auch die            #
 * # Prüfziffer 0. Die Stelle 10 der Kontonummer ist die Prüfziffer.    #
 * ######################################################################
 */
      case 6:
#if DEBUG>0
         if(retvals){
            retvals->methode="06";
            retvals->pz_methode=6;
         }
#endif
         pz = (kto[0]-'0') * 4
            + (kto[1]-'0') * 3
            + (kto[2]-'0') * 2
            + (kto[3]-'0') * 7
            + (kto[4]-'0') * 6
            + (kto[5]-'0') * 5
            + (kto[6]-'0') * 4
            + (kto[7]-'0') * 3
            + (kto[8]-'0') * 2;

         MOD_11_176;   /* pz%=11 */
         if(pz<=1)
            pz=0;
         else
            pz=11-pz;
         CHECK_PZ10;

/*  Berechnung nach der Methode 07 +§§§4 */
/*
 * ######################################################################
 * #               Berechnung nach der Methode 07                       #
 * ######################################################################
 * # Modulus 11, Gewichtung 2, 3, 4, 5, 6, 7, 8, 9, 10.                 #
 * # Die Berechnung erfolgt wie bei Verfahren 02.                       #
 * ######################################################################
 */
      case 7:
#if DEBUG>0
         if(retvals){
            retvals->methode="07";
            retvals->pz_methode=7;
         }
#endif
         pz = (kto[0]-'0') * 10
            + (kto[1]-'0') * 9
            + (kto[2]-'0') * 8
            + (kto[3]-'0') * 7
            + (kto[4]-'0') * 6
            + (kto[5]-'0') * 5
            + (kto[6]-'0') * 4
            + (kto[7]-'0') * 3
            + (kto[8]-'0') * 2;

         MOD_11_352;   /* pz%=11 */
         if(pz)pz=11-pz;
         INVALID_PZ10;
         CHECK_PZ10;

/*  Berechnung nach der Methode 08 +§§§4 */
/*
 * ######################################################################
 * #               Berechnung nach der Methode 08                       #
 * ######################################################################
 * # Modulus 10, Gewichtung 2, 1, 2, 1, 2, 1, 2, 1, 2 (modifiziert).    #
 * # Die Berechnung erfolgt wie bei Verfahren 00, jedoch erst           #
 * # ab der Kontonummer 60 000.                                         #
 * ######################################################################
 */
      case 8:
#if DEBUG>0
         if(retvals){
            retvals->methode="08";
            retvals->pz_methode=8;
         }
#endif
         if(strcmp(kto,"0000060000")<0)  /* Kontonummern unter 60 000: keine Prüfzifferberechnung oder falsch??? */
            return INVALID_KTO;
#ifdef __ALPHA
         pz = ((kto[0]<'5') ? (kto[0]-'0')*2 : (kto[0]-'0')*2-9)
            +  (kto[1]-'0')
            + ((kto[2]<'5') ? (kto[2]-'0')*2 : (kto[2]-'0')*2-9)
            +  (kto[3]-'0')
            + ((kto[4]<'5') ? (kto[4]-'0')*2 : (kto[4]-'0')*2-9)
            +  (kto[5]-'0')
            + ((kto[6]<'5') ? (kto[6]-'0')*2 : (kto[6]-'0')*2-9)
            +  (kto[7]-'0')
            + ((kto[8]<'5') ? (kto[8]-'0')*2 : (kto[8]-'0')*2-9);
#else
         pz=(kto[1]-'0')+(kto[3]-'0')+(kto[5]-'0')+(kto[7]-'0');
         if(kto[0]<'5')pz+=(kto[0]-'0')*2; else pz+=(kto[0]-'0')*2-9;
         if(kto[2]<'5')pz+=(kto[2]-'0')*2; else pz+=(kto[2]-'0')*2-9;
         if(kto[4]<'5')pz+=(kto[4]-'0')*2; else pz+=(kto[4]-'0')*2-9;
         if(kto[6]<'5')pz+=(kto[6]-'0')*2; else pz+=(kto[6]-'0')*2-9;
         if(kto[8]<'5')pz+=(kto[8]-'0')*2; else pz+=(kto[8]-'0')*2-9;
#endif
         MOD_10_80;   /* pz%=10 */
         if(pz)pz=10-pz;
         CHECK_PZ10;

/*  Berechnung nach der Methode 09 +§§§4 */
/*
 * ######################################################################
 * #               Berechnung nach der Methode 09                       #
 * ######################################################################
 * # Keine Prüfziffernberechung (es wird immer richtig zurückgegeben).  #
 * ######################################################################
 */
      case 9:
#if DEBUG>0
         if(retvals){
            retvals->methode="09";
            retvals->pz_methode=9;
         }
#endif
         return OK_NO_CHK;

/* Berechnungsmethoden 10 bis 19 +§§§3
   Berechnung nach der Methode 10 +§§§4 */
/*
 * ######################################################################
 * #               Berechnung nach der Methode 10                       #
 * ######################################################################
 * # Modulus 11, Gewichtung 2, 3, 4, 5, 6, 7, 8, 9, 10 (modifiziert).   #
 * # Die Berechnung erfolgt wie bei Verfahren 06.                       #
 * ######################################################################
 */
      case 10:
#if DEBUG>0
         if(retvals){
            retvals->methode="10";
            retvals->pz_methode=10;
         }
#endif
         pz = (kto[0]-'0') * 10
            + (kto[1]-'0') * 9
            + (kto[2]-'0') * 8
            + (kto[3]-'0') * 7
            + (kto[4]-'0') * 6
            + (kto[5]-'0') * 5
            + (kto[6]-'0') * 4
            + (kto[7]-'0') * 3
            + (kto[8]-'0') * 2;

         MOD_11_352;   /* pz%=11 */
         if(pz<=1)
            pz=0;
         else
            pz=11-pz;
         CHECK_PZ10;

/*  Berechnung nach der Methode 11 +§§§4 */
/*
 * ######################################################################
 * #              Berechnung nach der Methode 11                        #
 * ######################################################################
 * # Modulus 11, Gewichtung 2, 3, 4, 5, 6, 7, 8, 9, 10 (modifiziert).   #
 * # Die Berechnung erfolgt wie bei Verfahren 06. Beim Rechenergebnis   #
 * # 10 wird die Null jedoch durch eine 9 ersetzt.                      #
 * ######################################################################
 */
      case 11:
#if DEBUG>0
         if(retvals){
            retvals->methode="11";
            retvals->pz_methode=11;
         }
#endif
         pz = (kto[0]-'0') * 10
            + (kto[1]-'0') * 9
            + (kto[2]-'0') * 8
            + (kto[3]-'0') * 7
            + (kto[4]-'0') * 6
            + (kto[5]-'0') * 5
            + (kto[6]-'0') * 4
            + (kto[7]-'0') * 3
            + (kto[8]-'0') * 2;

         MOD_11_352;   /* pz%=11 */
         if(pz==1)
            pz=9;
         else if(pz>1)
            pz=11-pz;
         CHECK_PZ10;

/*  Berechnung nach der Methode 12 +§§§4 */
/*
 * ######################################################################
 * #              Berechnung nach der Methode 12                        #
 * ######################################################################
 * # frei/nicht definiert                                               #
 * # Beim Aufruf dieser Methode wird grundsätzlich ein Fehler zurück-   #
 * # gegeben, um nicht eine (evl. falsche) Implementation vorzutäuschen.#
 * ######################################################################
 */
      case 12:
#if DEBUG>0
         if(retvals){
            retvals->methode="12";
            retvals->pz_methode=12;
         }
#endif /* frei */
         return NOT_DEFINED;

/*  Berechnung nach der Methode 13 +§§§4 */
/*
 * ######################################################################
 * #              Berechnung nach der Methode 13                        #
 * ######################################################################
 * # Modulus 10, Gewichtung 2, 1, 2, 1, 2, 1.                           #
 * # Die Berechnung erfolgt wie bei Verfahren 00. Es ist jedoch zu      #
 * # beachten, daß die zweistellige Unterkonto-Nummer (Stellen 9        #
 * # und 10) nicht in das Prüfziffernberechnungsverfahren mit           #
 * # einbezogen werden darf. Die für die Berechnung relevante           #
 * # sechsstellige Grundnummer befindet sich in den Stellen 2 bis 7,    #
 * # die Prüfziffer in Stelle 8. Die Kontonummer ist neunstellig,       #
 * # Stelle 1 ist also unbenutzt.                                       #
 * # Ist die obengenannte Unternummer = 00 kommt es vor, daß sie        #
 * # auf den Zahlungsverkehrsbelegen nicht angegeben ist. Ergibt        #
 * # die erste Berechnung einen Prüfziffernfehler, wird empfohlen,      #
 * # die Prüfziffernberechnung ein zweites Mal durchzuführen und        #
 * # dabei die "gedachte" Unternummer 00 zu berücksichtigen.            #
 * ######################################################################
 */
      case 13:
#if DEBUG>0
      case 1013:
         if(retvals){
            retvals->methode="13a";
            retvals->pz_methode=1013;
         }
#endif
#ifdef __ALPHA
         pz =  (kto[1]-'0')
            + ((kto[2]<'5') ? (kto[2]-'0')*2 : (kto[2]-'0')*2-9)
            +  (kto[3]-'0')
            + ((kto[4]<'5') ? (kto[4]-'0')*2 : (kto[4]-'0')*2-9)
            +  (kto[5]-'0')
            + ((kto[6]<'5') ? (kto[6]-'0')*2 : (kto[6]-'0')*2-9);
#else
         pz =(kto[1]-'0')+(kto[3]-'0')+(kto[5]-'0');
         if(kto[2]<'5')pz+=(kto[2]-'0')*2; else pz+=(kto[2]-'0')*2-9;
         if(kto[4]<'5')pz+=(kto[4]-'0')*2; else pz+=(kto[4]-'0')*2-9;
         if(kto[6]<'5')pz+=(kto[6]-'0')*2; else pz+=(kto[6]-'0')*2-9;
#endif
         MOD_10_80;   /* pz%=10 */
         if(pz)pz=10-pz;
         CHECK_PZX8;

#if DEBUG>0
      case 2013:
         if(retvals){
            retvals->methode="13b";
            retvals->pz_methode=2013;
         }
#endif
#ifdef __ALPHA
         pz =  (kto[3]-'0')
            + ((kto[4]<'5') ? (kto[4]-'0')*2 : (kto[4]-'0')*2-9)
            +  (kto[5]-'0')
            + ((kto[6]<'5') ? (kto[6]-'0')*2 : (kto[6]-'0')*2-9)
            +  (kto[7]-'0')
            + ((kto[8]<'5') ? (kto[8]-'0')*2 : (kto[8]-'0')*2-9);
#else
         pz =(kto[3]-'0')+(kto[5]-'0')+(kto[7]-'0');
         if(kto[4]<'5')pz+=(kto[4]-'0')*2; else pz+=(kto[4]-'0')*2-9;
         if(kto[6]<'5')pz+=(kto[6]-'0')*2; else pz+=(kto[6]-'0')*2-9;
         if(kto[8]<'5')pz+=(kto[8]-'0')*2; else pz+=(kto[8]-'0')*2-9;
#endif

         MOD_10_80;   /* pz%=10 */
         if(pz)pz=10-pz;
         CHECK_PZ10;

/*  Berechnung nach der Methode 14 +§§§4 */
/*
 * ######################################################################
 * #              Berechnung nach der Methode 14                        #
 * ######################################################################
 * # Modulus 11, Gewichtung 2, 3, 4, 5, 6, 7.                           #
 * # Die Berechnung erfolgt wie bei Verfahren 02. Es ist jedoch zu      #
 * # beachten, daß die zweistellige Kontoart nicht in das Prüfziffern-  #
 * # berechnungsverfahren mit einbezogen wird. Die Kontoart belegt      #
 * # die Stellen 2 und 3, die zu berechnende Grundnummer die Stellen    #
 * # 4 bis 9. Die Prüfziffer befindet sich in Stelle 10.                #
 * ######################################################################
 */
      case 14:
#if DEBUG>0
         if(retvals){
            retvals->methode="14";
            retvals->pz_methode=14;
         }
#endif
         pz = (kto[3]-'0') * 7
            + (kto[4]-'0') * 6
            + (kto[5]-'0') * 5
            + (kto[6]-'0') * 4
            + (kto[7]-'0') * 3
            + (kto[8]-'0') * 2;

         MOD_11_176;   /* pz%=11 */
         if(pz)pz=11-pz;
         INVALID_PZ10;
         CHECK_PZ10;

/*  Berechnung nach der Methode 15 +§§§4 */
/*
 * ######################################################################
 * #              Berechnung nach der Methode 15                        #
 * ######################################################################
 * # Modulus 11, Gewichtung 2, 3, 4, 5.                                 #
 * # Die Berechnung erfolgt wie bei Verfahren 06. Es ist jedoch zu      #
 * # beachten, daß nur die vierstellige Kontonummer in das              #
 * # Prüfziffernberechnungsverfahren einbezogen wird. Sie befindet      #
 * # sich in den Stellen 6 bis 9, die Prüfziffer in Stelle 10           #
 * # der Kontonummer.                                                   #
 * ######################################################################
 */
      case 15:
#if DEBUG>0
         if(retvals){
            retvals->methode="15";
            retvals->pz_methode=15;
         }
#endif
         pz = (kto[5]-'0') * 5
            + (kto[6]-'0') * 4
            + (kto[7]-'0') * 3
            + (kto[8]-'0') * 2;

         MOD_11_88;    /* pz%=11 */
         if(pz<=1)
            pz=0;
         else
            pz=11-pz;
         CHECK_PZ10;

/*  Berechnung nach der Methode 16 +§§§4 */
/*
 * ######################################################################
 * #              Berechnung nach der Methode 16                        #
 * ######################################################################
 * # Modulus 11, Gewichtung 2, 3, 4, 5, 6, 7, 2, 3, 4                   #
 * # Die Berechnung erfolgt wie bei Verfahren 06. Sollte sich jedoch    #
 * # nach der Division der Rest 1 ergeben, so ist die Kontonummer       #
 * # unabhängig vom eigentlichen Berechnungsergebnis                    #
 * # richtig, wenn die Ziffern an 10. und 9. Stelle identisch sind.     #
 * ######################################################################
 */
      case 16:
#if DEBUG>0
         if(retvals){
            retvals->methode="16";
            retvals->pz_methode=16;
         }
#endif
         pz = (kto[0]-'0') * 4
            + (kto[1]-'0') * 3
            + (kto[2]-'0') * 2
            + (kto[3]-'0') * 7
            + (kto[4]-'0') * 6
            + (kto[5]-'0') * 5
            + (kto[6]-'0') * 4
            + (kto[7]-'0') * 3
            + (kto[8]-'0') * 2;

         MOD_11_176;   /* pz%=11 */
         if(pz)pz=11-pz;
         if(pz==10){
#if DEBUG>0
            if(retvals){
               retvals->pz_pos=10;
               retvals->pz=*(kto+8)-'0';
            }
#endif
            if(*(kto+8)== *(kto+9))
               return OK;
            else
               /* die Frage hier ist, was ist wenn 9. und 10. Stelle
                * unterschiedlich sind aber das Ergebnis von Verfahren 6
                * akzeptiert würde? Markus nimmt das Beispiel aus Verfahren 23
                * her (das auf Verfahren 16 aufbaut); da wird explizit gesagt,
                * falls die Stellen 6 und 7 (entsprechen hier den Stellen 9 und
                * 10) nicht übereinstimmen, ist das Konto als falsch zu werten.
                * Ich benutze den fallback zu Version 6 und nehme als
                * Prüfziffer 0. Die Frage müßte von den Banken geklärt werden,
                * der Text gibt es nicht her.
                */
#if BAV_KOMPATIBEL
               return BAV_FALSE;
#else
               pz=0;
#endif
         }
         CHECK_PZ10;

/*  Berechnung nach der Methode 17 +§§§4 */
/*
 * ######################################################################
 * #              Berechnung nach der Methode 17 (neu)                  #
 * ######################################################################
 * # Modulus 11, Gewichtung 1, 2, 1, 2, 1, 2                            #
 * # Die Kontonummer ist 10-stellig mit folgendem Aufbau:               #
 * #                                                                    #
 * #     KSSSSSSPUU                                                     #
 * #     K = Kontoartziffer                                             #
 * #     S = Stammnummer                                                #
 * #     P = Prüfziffer                                                 #
 * #     U = Unterkontonummer                                           #
 * #                                                                    #
 * # Die für die Berechnung relevante 6-stellige Stammnummer            #
 * # (Kundennummer) befindet sich in den Stellen 2 bis 7 der            #
 * # Kontonummer, die Prüfziffer in der Stelle 8. Die einzelnen         #
 * # Stellen der Stammnummer (S) sind von links nach rechts mit         #
 * # den Ziffern 1, 2, 1, 2, 1, 2 zu multiplizieren. Die                #
 * # jeweiligen Produkte sind zu addieren, nachdem aus eventuell        #
 * # zweistelligen Produkten der 2., 4. und 6. Stelle der               #
 * # Stammnummer die Quersumme gebildet wurde. Von der Summe ist        #
 * # der Wert "1" zu subtrahieren. Das Ergebnis ist dann durch          #
 * # 11 zu dividieren. Der verbleibende Rest wird von 10                #
 * # subtrahiert. Das Ergebnis ist die Prüfziffer. Verbleibt            #
 * # nach der Division durch 11 kein Rest, ist die Prüfziffer 0.        #
 * ######################################################################
 */

      case 17:
#if DEBUG>0
         if(retvals){
            retvals->methode="17";
            retvals->pz_methode=17;
         }
#endif
#ifdef __ALPHA
         pz =  (kto[1]-'0')
            + ((kto[2]<'5') ? (kto[2]-'0')*2 : (kto[2]-'0')*2-9)
            +  (kto[3]-'0')
            + ((kto[4]<'5') ? (kto[4]-'0')*2 : (kto[4]-'0')*2-9)
            +  (kto[5]-'0')
            + ((kto[6]<'5') ? (kto[6]-'0')*2 : (kto[6]-'0')*2-9);
#else
         pz =(kto[1]-'0')+(kto[3]-'0')+(kto[5]-'0');
         if(kto[2]<'5')pz+=(kto[2]-'0')*2; else pz+=(kto[2]-'0')*2-9;
         if(kto[4]<'5')pz+=(kto[4]-'0')*2; else pz+=(kto[4]-'0')*2-9;
         if(kto[6]<'5')pz+=(kto[6]-'0')*2; else pz+=(kto[6]-'0')*2-9;
#endif

         pz-=1;
         MOD_11_44;   /* pz%=11 */
         if(pz)pz=10-pz;
         CHECK_PZ8;

/*  Berechnung nach der Methode 18 +§§§4 */
/*
 * ######################################################################
 * #              Berechnung nach der Methode 18                        #
 * ######################################################################
 * # Modulus 10, Gewichtung 3, 9, 7, 1, 3, 9, 7, 1, 3.                  #
 * # Die Berechnung erfolgt wie bei Verfahren 01.                       #
 * ######################################################################
 */
      case 18:
#if DEBUG>0
         if(retvals){
            retvals->methode="18";
            retvals->pz_methode=18;
         }
#endif
         pz = (kto[0]-'0') * 3
            + (kto[1]-'0')
            + (kto[2]-'0') * 7
            + (kto[3]-'0') * 9
            + (kto[4]-'0') * 3
            + (kto[5]-'0')
            + (kto[6]-'0') * 7
            + (kto[7]-'0') * 9
            + (kto[8]-'0') * 3;

         MOD_10_320;   /* pz%=10 */
         if(pz)pz=10-pz;
         CHECK_PZ10;

/*  Berechnung nach der Methode 19 +§§§4 */
/*
 * ######################################################################
 * #              Berechnung nach der Methode 19                        #
 * ######################################################################
 * # Modulus 11, Gewichtung 2, 3, 4, 5, 6, 7, 8, 9, 1.                  #
 * # Die Berechnung erfolgt wie bei Verfahren 06.                       #
 * ######################################################################
 */
      case 19:
#if DEBUG>0
         if(retvals){
            retvals->methode="19";
            retvals->pz_methode=19;
         }
#endif
         pz = (kto[0]-'0')
            + (kto[1]-'0') * 9
            + (kto[2]-'0') * 8
            + (kto[3]-'0') * 7
            + (kto[4]-'0') * 6
            + (kto[5]-'0') * 5
            + (kto[6]-'0') * 4
            + (kto[7]-'0') * 3
            + (kto[8]-'0') * 2;

         MOD_11_352;   /* pz%=11 */
         if(pz<=1)
            pz=0;
         else
            pz=11-pz;
         CHECK_PZ10;

/* Berechnungsmethoden 20 bis 29 +§§§3
   Berechnung nach der Methode 20 +§§§4 */
/*
 * ######################################################################
 * #              Berechnung nach der Methode 20                        #
 * ######################################################################
 * # Modulus 11, Gewichtung 2, 3, 4, 5, 6, 7, 8, 9, 3 (modifiziert).    #
 * # Die Berechnung erfolgt wie bei Verfahren 06.                       #
 * ######################################################################
 */
      case 20:
#if DEBUG>0
         if(retvals){
            retvals->methode="20";
            retvals->pz_methode=20;
         }
#endif
         pz = (kto[0]-'0') * 3
            + (kto[1]-'0') * 9
            + (kto[2]-'0') * 8
            + (kto[3]-'0') * 7
            + (kto[4]-'0') * 6
            + (kto[5]-'0') * 5
            + (kto[6]-'0') * 4
            + (kto[7]-'0') * 3
            + (kto[8]-'0') * 2;

         MOD_11_352;   /* pz%=11 */
         if(pz<=1)
            pz=0;
         else
            pz=11-pz;
         CHECK_PZ10;

/*  Berechnung nach der Methode 21 +§§§4 */
/*
 * ######################################################################
 * #              Berechnung nach der Methode 21                        #
 * ######################################################################
 * # Modulus 10, Gewichtung 2, 1, 2, 1, 2, 1, 2, 1, 2 (modifiziert).    #
 * # Die Berechnung erfolgt wie bei Verfahren 00. Nach der Addition     #
 * # der Produkte werden neben der Einerstelle jedoch alle Stellen      #
 * # berücksichtigt, indem solange Quersummen gebildet werden, bis      #
 * # ein einstelliger Wert verbleibt. Die Differenz zwischen diesem     #
 * # Wert und dem Wert 10 ist die Prüfziffer.                           #
 * ######################################################################
 */
      case 21:
#if DEBUG>0
         if(retvals){
            retvals->methode="21";
            retvals->pz_methode=21;
         }
#endif
         pz = (kto[0]-'0') * 2
            + (kto[1]-'0')
            + (kto[2]-'0') * 2
            + (kto[3]-'0')
            + (kto[4]-'0') * 2
            + (kto[5]-'0')
            + (kto[6]-'0') * 2
            + (kto[7]-'0')
            + (kto[8]-'0') * 2;

         if(pz>=80)pz=pz-80+8;
         if(pz>=40)pz=pz-40+4;
         if(pz>=20)pz=pz-20+2;
         if(pz>=10)pz=pz-10+1;
         if(pz>=10)pz=pz-10+1;
         if(pz)pz=10-pz;
         CHECK_PZ10;

/*  Berechnung nach der Methode 22 +§§§4 */
/*
 * ######################################################################
 * #              Berechnung nach der Methode 22                        #
 * ######################################################################
 * # Modulus 10, Gewichtung 3, 1, 3, 1, 3, 1, 3, 1, 3.                  #
 * # Die einzelnen Stellen der Kontonummer sind von rechts nach         #
 * # links mit den Ziffern 3, 1, 3, 1 usw. zu multiplizieren.           #
 * # Von den jeweiligen Produkten bleiben die Zehnerstellen             #
 * # unberücksichtigt. Die verbleibenden Zahlen (Einerstellen)          #
 * # werden addiert. Die Differenz bis zum nächsten Zehner ist          #
 * # die Prüfziffer.                                                    #
 * ######################################################################
 */
      case 22:
#if DEBUG>0
         if(retvals){
            retvals->methode="22";
            retvals->pz_methode=22;
         }
#endif
         pz = (kto[1]-'0')+(kto[3]-'0')+(kto[5]-'0')+(kto[7]-'0');

            if(kto[0]<'4')
               pz+=(kto[0]-'0')*3;
            else if(kto[0]<'7')
               pz+=(kto[0]-'0')*3-10;
            else
               pz+=(kto[0]-'0')*3-20;

            if(kto[2]<'4')
               pz+=(kto[2]-'0')*3;
            else if(kto[2]<'7')
               pz+=(kto[2]-'0')*3-10;
            else
               pz+=(kto[2]-'0')*3-20;

            if(kto[4]<'4')
               pz+=(kto[4]-'0')*3;
            else if(kto[4]<'7')
               pz+=(kto[4]-'0')*3-10;
            else
               pz+=(kto[4]-'0')*3-20;

            if(kto[6]<'4')
               pz+=(kto[6]-'0')*3;
            else if(kto[6]<'7')
               pz+=(kto[6]-'0')*3-10;
            else
               pz+=(kto[6]-'0')*3-20;

            if(kto[8]<'4')
               pz+=(kto[8]-'0')*3;
            else if(kto[8]<'7')
               pz+=(kto[8]-'0')*3-10;
            else
               pz+=(kto[8]-'0')*3-20;

         MOD_10_80;   /* pz%=10 */
         if(pz)pz=10-pz;
         CHECK_PZ10;

/*  Berechnung nach der Methode 23 +§§§4 */
/*
 * ######################################################################
 * #     Berechnung nach der Methode 23 (geändert zum 3.9.2001)         #
 * ######################################################################
 * # Modulus 11, Gewichtung 2, 3, 4, 5, 6, 7.                           #
 * # Das Prüfziffernverfahren entspricht dem der Kennziffer 16,         #
 * # wird jedoch nur auf die ersten sechs Ziffern der Kontonummer       #
 * # angewandt. Die Prüfziffer befindet sich an der 7. Stelle der       #
 * # Kontonummer. Die drei folgenden Stellen bleiben ungeprüft.         #
 * # Sollte sich nach der Division der Rest 1 ergeben, so ist           #
 * # die Kontonummer unabhängig vom eigentlichen Berechnungsergebnis    #
 * # richtig, wenn die Ziffern an 6. und 7. Stelle identisch sind.      #
 * ######################################################################
 */
      case 23:
#if DEBUG>0
         if(retvals){
            retvals->methode="23";
            retvals->pz_methode=23;
         }
#endif
         pz = (kto[0]-'0') * 7
            + (kto[1]-'0') * 6
            + (kto[2]-'0') * 5
            + (kto[3]-'0') * 4
            + (kto[4]-'0') * 3
            + (kto[5]-'0') * 2;

         MOD_11_176;   /* pz%=11 */
         if(pz)pz=11-pz;
         if(pz==10){
#if DEBUG>0
            if(retvals){
               retvals->pz_pos=7;
               retvals->pz=*(kto+6)-'0';
            }
#endif
            if(*(kto+5)==*(kto+6))
               return OK;
            else
               return FALSE;
         }
         CHECK_PZ7;

/*  Berechnung nach der Methode 24 +§§§4 */
/*
 * ######################################################################
 * #              Berechnung nach der Methode 24                        #
 * ######################################################################
 * # Modulus 11, Gewichtung 1, 2, 3, 1, 2, 3, 1, 2, 3                   #
 * # Die für die Berechnung relevanten Stellen der Kontonummer          #
 * # befinden sich in den Stellen 1 - 9 und die Prüfziffer in           #
 * # Stelle 10 des zehnstelligen Kontonummernfeldes. Eine evtl.         #
 * # in Stelle 1 vorhandene Ziffer 3, 4, 5, 6 wird wie 0 gewertet.      #
 * # Eine ggf. in Stelle 1 vorhandene Ziffer 9 wird als 0 gewertet und  #
 * # führt dazu, dass auch die beiden nachfolgenden Ziffern in den      #
 * # Stellen 2 und 3 der Kontonummer als 0 gewertet werden müssen. Der  #
 * # o. g. Prüfalgorithmus greift in diesem Fall also erst ab Stelle 4  #
 * # der 10stelligen Kontonummer. Die Stelle 4 ist ungleich 0.          #
 * #                                                                    #
 * # Die einzelnen Stellen der Kontonummer sind von                     #
 * # links nach rechts, beginnend mit der ersten signifikanten          #
 * # Ziffer (Ziffer ungleich 0) in den Stellen 1 - 9, mit den           #
 * # Ziffern 1, 2, 3, 1, 2, 3, 1, 2, 3 zu multiplizieren. Zum           #
 * # jeweiligen Produkt ist die entsprechende Gewichtungsziffer         #
 * # zu addieren (zum ersten Produkt + 1; zum zweiten Produkt + 2;      #
 * # zum dritten Produkt +3; zum vierten Produkt + 1 usw.).             #
 * # Die einzelnen Rechenergebnisse sind durch 11 zu dividieren.        #
 * # Die sich nach der Division ergebenden Reste sind zu summieren.     #
 * # Die Summe der Reste ist durch 10 zu dividieren. Der sich           #
 * # danach ergebende Rest ist die Prüfziffer.                          #
 * ######################################################################
 */
      case 24:
#if DEBUG>0
         if(retvals){
            retvals->methode="24";
            retvals->pz_methode=24;
         }
#endif
         if(*kto>='3' && *kto<='6')*kto='0';
         if(*kto=='9')*kto= *(kto+1)= *(kto+2)='0';
         for(ptr=kto;*ptr=='0';ptr++);
         for(i=0,pz=0;ptr<kto+9;ptr++,i++){
            p1=(*ptr-'0')*w24[i]+w24[i];
            SUB1_22; /* p1%=11 */
            pz+=p1;
         }
         MOD_10_80;     /* pz%=10 */
         CHECK_PZ10;

/*  Berechnung nach der Methode 25 +§§§4 */
/*
 * ######################################################################
 * #              Berechnung nach der Methode 25                        #
 * ######################################################################
 * # Modulus 11, Gewichtung 2, 3, 4, 5, 6, 7, 8, 9 ohne Quersumme.      #
 * # Die einzelnen Stellen der Kontonummer sind von rechts nach         #
 * # links mit den Ziffern 2, 3, 4, 5, 6, 7, 8, 9 zu multiplizieren.    #
 * # Die jeweiligen Produkte werden addiert. Die Summe ist durch        #
 * # 11 zu dividieren. Der verbleibende Rest wird vom Divisor (11)      #
 * # subtrahiert. Das Ergebnis ist die Prüfziffer. Verbleibt nach       #
 * # der Division durch 11 kein Rest, ist die Prüfziffer 0.             #
 * # Ergibt sich als Rest 1, ist die Prüfziffer immer 0 und kann        #
 * # nur für die Arbeitsziffer 8 und 9 verwendet werden. Die            #
 * # Kontonummer ist für die Arbeitsziffer 0, 1, 2, 3, 4, 5, 6          #
 * # und 7 dann nicht verwendbar.                                       #
 * # Die Arbeitsziffer (Geschäftsbereich oder Kontoart) befindet        #
 * # sich in der 2. Stelle (von links) des zehnstelligen                #
 * # Kontonummernfeldes.                                                #
 * ######################################################################
 */
      case 25:
#if DEBUG>0
         if(retvals){
            retvals->methode="25";
            retvals->pz_methode=25;
         }
#endif
         pz = (kto[1]-'0') * 9
            + (kto[2]-'0') * 8
            + (kto[3]-'0') * 7
            + (kto[4]-'0') * 6
            + (kto[5]-'0') * 5
            + (kto[6]-'0') * 4
            + (kto[7]-'0') * 3
            + (kto[8]-'0') * 2;

         MOD_11_352;   /* pz%=11 */
         if(pz)pz=11-pz;
         if(pz==10){
            pz=0;
            if(*(kto+1)!='8' && *(kto+1)!='9')return INVALID_KTO;
         }
         CHECK_PZ10;

/*  Berechnung nach der Methode 26 +§§§4 */
/*
 * ######################################################################
 * #              Berechnung nach der Methode 26                        #
 * ######################################################################
 * # Modulus 11. Gewichtung 2, 3, 4, 5, 6, 7, 2                         #
 * # Die Kontonummer ist 10-stellig. Sind Stelle 1 und 2 mit            #
 * # Nullen gefüllt ist die Kontonummer um 2 Stellen nach links         #
 * # zu schieben und Stelle 9 und 10 mit Nullen zu füllen. Die          #
 * # Berechnung erfolgt wie bei Verfahren 06 mit folgender              #
 * # Modifizierung: für die Berechnung relevant sind die Stellen 1      #
 * # - 7; die Prüfziffer steht in Stelle 8. Bei den Stellen 9 und 10    #
 * # handelt es sich um eine Unterkontonummer, welche für die           #
 * # Berechnung nicht berücksichtigt wird.                              #
 * ######################################################################
 */
      case 26:
#if DEBUG>0
         if(retvals){
            retvals->methode="26";
            retvals->pz_methode=26;
         }
#endif
         if(*kto=='0' && *(kto+1)=='0'){
         pz = (kto[2]-'0') * 2
            + (kto[3]-'0') * 7
            + (kto[4]-'0') * 6
            + (kto[5]-'0') * 5
            + (kto[6]-'0') * 4
            + (kto[7]-'0') * 3
            + (kto[8]-'0') * 2;

         MOD_11_176;   /* pz%=11 */
            if(pz<=1)
               pz=0;
            else
               pz=11-pz;
            CHECK_PZ10;
         }
         else{
            pz = (kto[0]-'0') * 2
               + (kto[1]-'0') * 7
               + (kto[2]-'0') * 6
               + (kto[3]-'0') * 5
               + (kto[4]-'0') * 4
               + (kto[5]-'0') * 3
               + (kto[6]-'0') * 2;

            MOD_11_176;   /* pz%=11 */
            if(pz<=1)
               pz=0;
            else
               pz=11-pz;
            CHECK_PZ8;
         }

/*  Berechnung nach der Methode 27 +§§§4 */
/*
 * ######################################################################
 * #              Berechnung nach der Methode 27                        #
 * ######################################################################
 * # Modulus 10, Gewichtung 2, 1, 2, 1, 2, 1, 2, 1, 2 (modifiziert).    #
 * # Die Berechnung erfolgt wie bei Verfahren 00, jedoch nur für        #
 * # die Kontonummern von 1 bis 999.999.999. Ab Konto 1.000.000.000     #
 * # kommt das Prüfziffernverfahren M10H (Iterierte Transformation)     #
 * # zum Einsatz.                                                       #
 * # Es folgt die Beschreibung der iterierten Transformation:           #
 * # Die Position der einzelnen Ziffer von rechts nach links            #
 * # innerhalb der Kontonummer gibt die Zeile 1 bis 4 der               #
 * # Transformationstabelle an. Aus ihr sind die Übersetzungswerte      #
 * # zu summieren. Die Einerstelle wird von 10 subtrahiert und          #
 * # stellt die Prüfziffer dar.                                         #
 * # Transformationstabelle:                                            #
 * #    Ziffer    0 1 2 3 4 5 6 7 8 9                                   #
 * #    Ziffer 1  0 1 5 9 3 7 4 8 2 6                                   #
 * #    Ziffer 2  0 1 7 6 9 8 3 2 5 4                                   #
 * #    Ziffer 3  0 1 8 4 6 2 9 5 7 3                                   #
 * #    Ziffer 4  0 1 2 3 4 5 6 7 8 9                                   #
 * ######################################################################
 */
      case 27:
#if DEBUG>0
         if(retvals){
            retvals->methode="27";
            retvals->pz_methode=27;
         }
#endif
         if(*kto=='0'){ /* Kontonummern von 1 bis 999.999.999 */
#ifdef __ALPHA
         pz =  (kto[1]-'0')
            + ((kto[2]<'5') ? (kto[2]-'0')*2 : (kto[2]-'0')*2-9)
            +  (kto[3]-'0')
            + ((kto[4]<'5') ? (kto[4]-'0')*2 : (kto[4]-'0')*2-9)
            +  (kto[5]-'0')
            + ((kto[6]<'5') ? (kto[6]-'0')*2 : (kto[6]-'0')*2-9)
            +  (kto[7]-'0')
            + ((kto[8]<'5') ? (kto[8]-'0')*2 : (kto[8]-'0')*2-9);
#else
         pz =(kto[1]-'0')+(kto[3]-'0')+(kto[5]-'0')+(kto[7]-'0');
         if(kto[2]<'5')pz+=(kto[2]-'0')*2; else pz+=(kto[2]-'0')*2-9;
         if(kto[4]<'5')pz+=(kto[4]-'0')*2; else pz+=(kto[4]-'0')*2-9;
         if(kto[6]<'5')pz+=(kto[6]-'0')*2; else pz+=(kto[6]-'0')*2-9;
         if(kto[8]<'5')pz+=(kto[8]-'0')*2; else pz+=(kto[8]-'0')*2-9;
#endif
         MOD_10_80;   /* pz%=10 */
         }
         else{
            pz = m10h_digits[0][(unsigned int)(kto[0]-'0')]
               + m10h_digits[3][(unsigned int)(kto[1]-'0')]
               + m10h_digits[2][(unsigned int)(kto[2]-'0')]
               + m10h_digits[1][(unsigned int)(kto[3]-'0')]
               + m10h_digits[0][(unsigned int)(kto[4]-'0')]
               + m10h_digits[3][(unsigned int)(kto[5]-'0')]
               + m10h_digits[2][(unsigned int)(kto[6]-'0')]
               + m10h_digits[1][(unsigned int)(kto[7]-'0')]
               + m10h_digits[0][(unsigned int)(kto[8]-'0')];
            MOD_10_80;   /* pz%=10 */
         }
         if(pz)pz=10-pz;
         CHECK_PZ10;

/*  Berechnung nach der Methode 28 +§§§4 */
/*
 * ######################################################################
 * #              Berechnung nach der Methode 28                        #
 * ######################################################################
 * # Modulus 11, Gewichtung 2, 3, 4, 5, 6, 7, 8.                        #
 * # Innerhalb der 10stelligen Kontonummer ist die 8. Stelle die        #
 * # Prüfziffer. Die 9. und 10. Stelle der Kontonummer sind             #
 * # Unterkontonummern, die nicht in die Prüfziffernberechnung          #
 * # einbezogen sind.                                                   #
 * # Jede Stelle der Konto-Stamm-Nummer wird mit einem festen           #
 * # Stellenfaktor (Reihenfolge 8, 7, 6, 5, 4, 3, 2) multipliziert.     #
 * # Die sich ergebenden Produkte werden addiert. Die aus der           #
 * # Addition erhaltene Summe wird durch 11 dividiert. Der Rest         #
 * # wird von 11 subtrahiert. Die Differenz wird der Konto-Stamm-       #
 * # Nummer als Prüfziffer beigefügt. Wird als Rest eine 0 oder         #
 * # eine 1 ermittelt, so lautet die Prüfziffer 0.                      #
 * ######################################################################
 */
      case 28:
#if DEBUG>0
         if(retvals){
            retvals->methode="28";
            retvals->pz_methode=28;
         }
#endif
         pz = (kto[0]-'0') * 8
            + (kto[1]-'0') * 7
            + (kto[2]-'0') * 6
            + (kto[3]-'0') * 5
            + (kto[4]-'0') * 4
            + (kto[5]-'0') * 3
            + (kto[6]-'0') * 2;

         MOD_11_176;   /* pz%=11 */
         if(pz<=1)
            pz=0;
         else
            pz=11-pz;
         CHECK_PZX8;
            /* evl. wurde eine Unterkontonummer weggelassen => nur 8-stellige
             * Kontonummer; neuer Versuch mit den Stellen ab der 3. Stelle
             */
         if(kto_len==8){
            pz = (kto[2]-'0') * 8
               + (kto[3]-'0') * 7
               + (kto[4]-'0') * 6
               + (kto[5]-'0') * 5
               + (kto[6]-'0') * 4
               + (kto[7]-'0') * 3
               + (kto[8]-'0') * 2;

            MOD_11_176;   /* pz%=11 */
            if(pz<=1)
               pz=0;
            else
               pz=11-pz;
            CHECK_PZ10;
         }
         else
            return FALSE;


/*  Berechnung nach der Methode 29 +§§§4 */
/*
 * ######################################################################
 * #              Berechnung nach der Methode 29                        #
 * ######################################################################
 * # Modulus 10, Iterierte Transformation.                              #
 * # Die einzelnen Ziffern der Kontonummer werden über eine Tabelle     #
 * # in andere Werte transformiert. Jeder einzelnen Stelle der          #
 * # Kontonummer ist hierzu eine der Zeilen 1 bis 4 der Transforma-     #
 * # tionstabelle fest zugeordnet. Die Transformationswerte werden      #
 * # addiert. Die Einerstelle der Summe wird von 10 subtrahiert.        #
 * # Das Ergebnis ist die Prüfziffer. Ist das Ergebnis = 10, ist        #
 * # die Prüfziffer = 0.                                                #
 * # Transformationstabelle:                                            #
 * #    Ziffer    0 1 2 3 4 5 6 7 8 9                                   #
 * #    Ziffer 1  0 1 5 9 3 7 4 8 2 6                                   #
 * #    Ziffer 2  0 1 7 6 9 8 3 2 5 4                                   #
 * #    Ziffer 3  0 1 8 4 6 2 9 5 7 3                                   #
 * #    Ziffer 4  0 1 2 3 4 5 6 7 8 9                                   #
 * ######################################################################
 */
      case 29:
#if DEBUG>0
         if(retvals){
            retvals->methode="29";
            retvals->pz_methode=29;
         }
#endif
         pz = m10h_digits[0][(unsigned int)(kto[0]-'0')]
            + m10h_digits[3][(unsigned int)(kto[1]-'0')]
            + m10h_digits[2][(unsigned int)(kto[2]-'0')]
            + m10h_digits[1][(unsigned int)(kto[3]-'0')]
            + m10h_digits[0][(unsigned int)(kto[4]-'0')]
            + m10h_digits[3][(unsigned int)(kto[5]-'0')]
            + m10h_digits[2][(unsigned int)(kto[6]-'0')]
            + m10h_digits[1][(unsigned int)(kto[7]-'0')]
            + m10h_digits[0][(unsigned int)(kto[8]-'0')];
         MOD_10_80;   /* pz%=10 */
         if(pz)pz=10-pz;
         CHECK_PZ10;

/* Berechnungsmethoden 30 bis 39 +§§§3
   Berechnung nach der Methode 30 +§§§4 */
/*
 * ######################################################################
 * #              Berechnung nach der Methode 30                        #
 * ######################################################################
 * # Modulus 10, Gewichtung 2, 0, 0, 0, 0, 1, 2, 1, 2.                  #
 * # Die letzte Stelle ist per Definition die Prüfziffer. Die           #
 * # einzelnen Stellen der Kontonummer sind ab der ersten Stelle von    #
 * # links nach rechts mit den Ziffern 2, 0, 0, 0, 0, 1, 2, 1, 2 zu     #
 * # multiplizieren. Die jeweiligen Produkte werden addiert (ohne       #
 * # Quersummenbildung). Die weitere Berechnung erfolgt wie bei         #
 * # Verfahren 00.                                                      #
 * ######################################################################
 */
      case 30:
#if DEBUG>0
         if(retvals){
            retvals->methode="30";
            retvals->pz_methode=30;
         }
#endif
         pz = (kto[0]-'0') * 2
            + (kto[5]-'0')
            + (kto[6]-'0') * 2
            + (kto[7]-'0')
            + (kto[8]-'0') * 2;

         MOD_10_40;   /* pz%=10 */
         if(pz)pz=10-pz;
         CHECK_PZ10;

/*  Berechnung nach der Methode 31 +§§§4 */
/*
 * ######################################################################
 * #              Berechnung nach der Methode 31                        #
 * ######################################################################
 * # Modulus 11, Gewichtung 9, 8, 7, 6, 5, 4, 3, 2, 1                   #
 * # Die Kontonummer ist 10-stellig. Die Stellen 1 bis 9 der            #
 * # Kontonummer sind von rechts nach links mit den Ziffern 9,          #
 * # 8, 7, 6, 5, 4, 3, 2, 1 zu multiplizieren. Die jeweiligen Produkte  #
 * # werden addiert. Die Summe ist durch 11 zu dividieren. Der          #
 * # verbleibende Rest ist die Prüfziffer. Verbleibt nach der           #
 * # Division durch 11 kein Rest, ist die Prüfziffer 0. Ergibt sich ein #
 * # Rest 10, ist die Kontonummer falsch.Die Prüfziffer  befindet sich  #
 * # in der 10. Stelle der Kontonummer.                                 #
 * ######################################################################
 */
      case 31:
#if DEBUG>0
         if(retvals){
            retvals->methode="31";
            retvals->pz_methode=31;
         }
#endif
         pz = (kto[0]-'0')
            + (kto[1]-'0') * 2
            + (kto[2]-'0') * 3
            + (kto[3]-'0') * 4
            + (kto[4]-'0') * 5
            + (kto[5]-'0') * 6
            + (kto[6]-'0') * 7
            + (kto[7]-'0') * 8
            + (kto[8]-'0') * 9;

         MOD_11_352;   /* pz%=11 */
         if(pz==10){
            pz= *(kto+9)+1;
            return INVALID_KTO;
         }
         CHECK_PZ10;

/*  Berechnung nach der Methode 32 +§§§4 */
/*
 * ######################################################################
 * #              Berechnung nach der Methode 32                        #
 * ######################################################################
 * # Modulus 11, Gewichtung 2, 3, 4, 5, 6, 7.                           #
 * # Die Kontonummer ist 10-stellig. Die einzelnen Stellen der          #
 * # Kontonummer werden von links nach rechts aufsteigend von           #
 * # 1 bis 10 durchnumeriert. Die Stelle 10 der Kontonummer ist         #
 * # per Definition die Prüfziffer.                                     #
 * # Es wird das Berechnungsverfahren 06 in modifizierter Form          #
 * # auf die Stellen 4 bis 9 angewendet. Die Gewichtung ist             #
 * # 2, 3, 4, 5, 6, 7. Die genannten Stellen werden von rechts          #
 * # nach links mit diesen Faktoren multipliziert. Die restliche        #
 * # Berechnung und mögliche Ergebnisse entsprechen dem Verfahren 06.   #
 * ######################################################################
 */
      case 32:
#if DEBUG>0
         if(retvals){
            retvals->methode="32";
            retvals->pz_methode=32;
         }
#endif
         pz = (kto[3]-'0') * 7
            + (kto[4]-'0') * 6
            + (kto[5]-'0') * 5
            + (kto[6]-'0') * 4
            + (kto[7]-'0') * 3
            + (kto[8]-'0') * 2;

         MOD_11_176;   /* pz%=11 */
         if(pz<=1)
            pz=0;
         else
            pz=11-pz;
         CHECK_PZ10;

/*  Berechnung nach der Methode 33 +§§§4 */
/*
 * ######################################################################
 * #              Berechnung nach der Methode 33                        #
 * ######################################################################
 * # Modulus 11, Gewichtung 2, 3, 4, 5, 6.                              #
 * # Die Kontonummer ist 10-stellig. Die einzelnen Stellen der          #
 * # Kontonummer werden von links nach rechts aufsteigend von           #
 * # 1 bis 10 durchnumeriert. Die Stelle 10 der Kontonummer ist         #
 * # per Definition die Prüfziffer.                                     #
 * # Es wird das Berechnungsverfahren 06 in modifizierter Form          #
 * # auf die Stellen 5 bis 9 angewendet. Die Gewichtung ist             #
 * # 2, 3, 4, 5, 6. Die genannten Stellen werden von rechts             #
 * # nach links mit diesen Faktoren multipliziert. Die restliche        #
 * # Berechnung und mögliche Ergebnisse entsprechen dem Verfahren 06.   #
 * ######################################################################
 */
      case 33:
#if DEBUG>0
         if(retvals){
            retvals->methode="33";
            retvals->pz_methode=33;
         }
#endif
         pz = (kto[4]-'0') * 6
            + (kto[5]-'0') * 5
            + (kto[6]-'0') * 4
            + (kto[7]-'0') * 3
            + (kto[8]-'0') * 2;

         MOD_11_176;   /* pz%=11 */
         if(pz<=1)
            pz=0;
         else
            pz=11-pz;
         CHECK_PZ10;

/*  Berechnung nach der Methode 34 +§§§4 */
/*
 * ######################################################################
 * #              Berechnung nach der Methode 34                        #
 * ######################################################################
 * # Modulus 11, Gewichtung 2, 4, 8, 5, A, 9, 7.                        #
 * # Die Kontonummer ist 10-stellig. Es wird das Berechnungsverfahren   #
 * # 28 mit modifizierter Gewichtung angewendet. Die Gewichtung         #
 * # lautet: 2, 4, 8, 5, A, 9, 7. Dabei steht der Buchstabe A für       #
 * # den Wert 10.                                                       #
 * ######################################################################
 */
      case 34:
#if DEBUG>0
         if(retvals){
            retvals->methode="34";
            retvals->pz_methode=34;
         }
#endif
         pz = (kto[0]-'0') * 7
            + (kto[1]-'0') * 9
            + (kto[2]-'0') * 10
            + (kto[3]-'0') * 5
            + (kto[4]-'0') * 8
            + (kto[5]-'0') * 4
            + (kto[6]-'0') * 2;

         MOD_11_352;   /* pz%=11 */
         if(pz<=1)
            pz=0;
         else
            pz=11-pz;
         CHECK_PZX8;

            /* evl. wurde eine Unterkontonummer weggelassen => nur 8-stellige
             * Kontonummer; neuer Versuch mit den Stellen ab der 3. Stelle
             */
         if(kto_len==8){
            pz = (kto[2]-'0') * 7
               + (kto[3]-'0') * 9
               + (kto[4]-'0') * 10
               + (kto[5]-'0') * 5
               + (kto[6]-'0') * 8
               + (kto[7]-'0') * 4
               + (kto[8]-'0') * 2;

            MOD_11_352;   /* pz%=11 */
            if(pz<=1)
               pz=0;
            else
               pz=11-pz;
            CHECK_PZ10;
         }
         else
            return FALSE;

/*  Berechnung nach der Methode 35 +§§§4 */
/*
 * ######################################################################
 * #              Berechnung nach der Methode 35                        #
 * ######################################################################
 * # Modulus 11, Gewichtung 2, 3, 4, 5, 6, 7, 8, 9, 10                  #
 * # Die Kontonummer ist ggf. durch linksbündige Nullenauffüllung       #
 * # 10-stellig darzustellen. Die 10. Stelle der Kontonummer ist die    #
 * # Prüfziffer. Die Stellen 1 bis 9 der Kontonummer werden von         #
 * # rechts nach links mit den Ziffern 2, 3, 4, ff. multipliziert. Die  #
 * # jeweiligen Produkte werden addiert. Die Summe der Produkte         #
 * # ist durch 11 zu dividieren. Der verbleibende Rest ist die          #
 * # Prüfziffer. Sollte jedoch der Rest 10 ergeben, so ist die          #
 * # Kontonummer unabhängig vom eigentlichen Berechnungsergebnis        #
 * # richtig, wenn die Ziffern an 10. und 9. Stelle identisch           #
 * # sind.                                                              #
 * ######################################################################
 */
      case 35:
#if DEBUG>0
         if(retvals){
            retvals->methode="35";
            retvals->pz_methode=35;
         }
#endif
         pz = (kto[0]-'0') * 10
            + (kto[1]-'0') * 9
            + (kto[2]-'0') * 8
            + (kto[3]-'0') * 7
            + (kto[4]-'0') * 6
            + (kto[5]-'0') * 5
            + (kto[6]-'0') * 4
            + (kto[7]-'0') * 3
            + (kto[8]-'0') * 2;

         MOD_11_352;   /* pz%=11 */
         if(pz==10){
            if(*(kto+8)== *(kto+9)){
               pz= *(kto+9)-'0';
               return OK;
            }
            else
               return INVALID_KTO;
         }
         CHECK_PZ10;

/*  Berechnung nach der Methode 36 +§§§4 */
/*
 * ######################################################################
 * #              Berechnung nach der Methode 36                        #
 * ######################################################################
 * # Modulus 11, Gewichtung 2, 4, 8, 5.                                 #
 * # Die Kontonummer ist 10-stellig. Die einzelnen Stellen der          #
 * # Kontonummer werden von links nach rechts aufsteigend von           #
 * # 1 bis 10 durchnumeriert. Die Stelle 10 der Kontonummer ist         #
 * # per Definition die Prüfziffer.                                     #
 * # Es wird das Berechnungsverfahren 06 in modifizierter Form          #
 * # auf die Stellen 6 bis 9 angewendet. Die Gewichtung ist             #
 * # 2, 4, 8, 5. Die genannten Stellen werden von rechts nach links     #
 * # mit diesen Faktoren multipliziert. Die restliche Berechnung        #
 * # und mögliche Ergebnisse entsprechen dem Verfahren 06.              #
 * ######################################################################
 */
      case 36:
#if DEBUG>0
         if(retvals){
            retvals->methode="36";
            retvals->pz_methode=36;
         }
#endif
         pz = (kto[5]-'0') * 5
            + (kto[6]-'0') * 8
            + (kto[7]-'0') * 4
            + (kto[8]-'0') * 2;

         MOD_11_88;   /* pz%=11 */
         if(pz<=1)
            pz=0;
         else
            pz=11-pz;
         CHECK_PZ10;

/*  Berechnung nach der Methode 37 +§§§4 */
/*
 * ######################################################################
 * #              Berechnung nach der Methode 37                        #
 * ######################################################################
 * # Modulus 11, Gewichtung 2, 4, 8, 5, A.                              #
 * # Die Kontonummer ist 10-stellig. Die einzelnen Stellen der          #
 * # Kontonummer werden von links nach rechts aufsteigend von           #
 * # 1 bis 10 durchnumeriert. Die Stelle 10 der Kontonummer ist         #
 * # per Definition die Prüfziffer.                                     #
 * # Es wird das Berechnungsverfahren 06 in modifizierter Form          #
 * # auf die Stellen 5 bis 9 angewendet. Die Gewichtung ist             #
 * # 2, 4, 8, 5, A. Dabei steht der Buchstabe A für den                 #
 * # Wert 10. Die genannten Stellen werden von rechts nach links        #
 * # mit diesen Faktoren multipliziert. Die restliche Berechnung        #
 * # und mögliche Ergebnisse entsprechen dem Verfahren 06.              #
 * ######################################################################
 */
      case 37:
#if DEBUG>0
         if(retvals){
            retvals->methode="37";
            retvals->pz_methode=37;
         }
#endif
         pz = (kto[4]-'0') * 10
            + (kto[5]-'0') * 5
            + (kto[6]-'0') * 8
            + (kto[7]-'0') * 4
            + (kto[8]-'0') * 2;

         MOD_11_176;   /* pz%=11 */
         if(pz<=1)
            pz=0;
         else
            pz=11-pz;
         CHECK_PZ10;

/*  Berechnung nach der Methode 38 +§§§4 */
/*
 * ######################################################################
 * #              Berechnung nach der Methode 38                        #
 * ######################################################################
 * # Modulus 11, Gewichtung 2, 4, 8, 5, A, 9.                           #
 * # Die Kontonummer ist 10-stellig. Die einzelnen Stellen der          #
 * # Kontonummer werden von links nach rechts aufsteigend von           #
 * # 1 bis 10 durchnumeriert. Die Stelle 10 der Kontonummer ist         #
 * # per Definition die Prüfziffer.                                     #
 * # Es wird das Berechnungsverfahren 06 in modifizierter Form          #
 * # auf die Stellen 4 bis 9 angewendet. Die Gewichtung ist             #
 * # 2, 4, 8, 5, A, 9. Dabei steht der Buchstabe A für den              #
 * # Wert 10. Die genannten Stellen werden von rechts nach links        #
 * # mit diesen Faktoren multipliziert. Die restliche Berechnung        #
 * # und mögliche Ergebnisse entsprechen dem Verfahren 06.              #
 * ######################################################################
 */
      case 38:
#if DEBUG>0
         if(retvals){
            retvals->methode="38";
            retvals->pz_methode=38;
         }
#endif
         pz = (kto[3]-'0') * 9
            + (kto[4]-'0') * 10
            + (kto[5]-'0') * 5
            + (kto[6]-'0') * 8
            + (kto[7]-'0') * 4
            + (kto[8]-'0') * 2;

         MOD_11_176;   /* pz%=11 */
         if(pz<=1)
            pz=0;
         else
            pz=11-pz;
         CHECK_PZ10;

/*  Berechnung nach der Methode 39 +§§§4 */
/*
 * ######################################################################
 * #              Berechnung nach der Methode 39                        #
 * ######################################################################
 * # Modulus 11, Gewichtung 2, 4, 8, 5, A, 9, 7.                        #
 * # Die Kontonummer ist 10-stellig. Die einzelnen Stellen der          #
 * # Kontonummer werden von links nach rechts aufsteigend von           #
 * # 1 bis 10 durchnumeriert. Die Stelle 10 der Kontonummer ist         #
 * # per Definition die Prüfziffer.                                     #
 * # Es wird das Berechnungsverfahren 06 in modifizierter Form          #
 * # auf die Stellen 3 bis 9 angewendet. Die Gewichtung ist             #
 * # 2, 4, 8, 5, A, 9, 7. Dabei steht der Buchstabe A für den           #
 * # Wert 10. Die genannten Stellen werden von rechts nach links        #
 * # mit diesen Faktoren multipliziert. Die restliche Berechnung        #
 * # und mögliche Ergebnisse entsprechen dem Verfahren 06.              #
 * ######################################################################
 */
      case 39:
#if DEBUG>0
         if(retvals){
            retvals->methode="39";
            retvals->pz_methode=39;
         }
#endif
         pz = (kto[2]-'0') * 7
            + (kto[3]-'0') * 9
            + (kto[4]-'0') * 10
            + (kto[5]-'0') * 5
            + (kto[6]-'0') * 8
            + (kto[7]-'0') * 4
            + (kto[8]-'0') * 2;

         MOD_11_352;   /* pz%=11 */
         if(pz<=1)
            pz=0;
         else
            pz=11-pz;
         CHECK_PZ10;

/* Berechnungsmethoden 40 bis 49 +§§§3
   Berechnung nach der Methode 40 +§§§4 */
/*
 * ######################################################################
 * #              Berechnung nach der Methode 40                        #
 * ######################################################################
 * # Modulus 11, Gewichtung 2, 4, 8, 5, A, 9, 7, 3, 6.                  #
 * # Die Kontonummer ist 10-stellig. Die einzelnen Stellen der          #
 * # Kontonummer werden von links nach rechts aufsteigend von           #
 * # 1 bis 10 durchnumeriert. Die Stelle 10 der Kontonummer ist         #
 * # per Definition die Prüfziffer.                                     #
 * # Es wird das Berechnungsverfahren 06 in modifizierter Form          #
 * # auf die Stellen 1 bis 9 angewendet. Die Gewichtung ist             #
 * # 2, 4, 8, 5, A, 9, 7, 3, 6. Dabei steht der Buchstabe A für den     #
 * # Wert 10. Die genannten Stellen werden von rechts nach links        #
 * # mit diesen Faktoren multipliziert. Die restliche Berechnung        #
 * # und mögliche Ergebnisse entsprechen dem Verfahren 06.              #
 * ######################################################################
 */
      case 40:
#if DEBUG>0
         if(retvals){
            retvals->methode="40";
            retvals->pz_methode=40;
         }
#endif
         pz = (kto[0]-'0') * 6
            + (kto[1]-'0') * 3
            + (kto[2]-'0') * 7
            + (kto[3]-'0') * 9
            + (kto[4]-'0') * 10
            + (kto[5]-'0') * 5
            + (kto[6]-'0') * 8
            + (kto[7]-'0') * 4
            + (kto[8]-'0') * 2;

         MOD_11_352;   /* pz%=11 */
         if(pz<=1)
            pz=0;
         else
            pz=11-pz;
         CHECK_PZ10;

/*  Berechnung nach der Methode 41 +§§§4 */
/*
 * ######################################################################
 * #              Berechnung nach der Methode 41                        #
 * ######################################################################
 * # Modulus 10, Gewichtung 2, 1, 2, 1, 2, 1, 2, 1, 2 (modifiziert)     #
 * # Die Berechnung erfolgt wie bei Verfahren 00                        #
 * # Ausnahme:                                                          #
 * # Ist die 4. Stelle der Kontonummer (von links) = 9, so werden       #
 * # die Stellen 1 bis 3 nicht in die Prüfzifferberechnung ein-bezogen. #
 * ######################################################################
 */
      case 41:
#if DEBUG>0
         if(retvals){
            retvals->methode="41";
            retvals->pz_methode=41;
         }
#endif
         if(*(kto+3)=='9'){
#ifdef __ALPHA
            pz =  (kto[3]-'0')
               + ((kto[4]<'5') ? (kto[4]-'0')*2 : (kto[4]-'0')*2-9)
               +  (kto[5]-'0')
               + ((kto[6]<'5') ? (kto[6]-'0')*2 : (kto[6]-'0')*2-9)
               +  (kto[7]-'0')
               + ((kto[8]<'5') ? (kto[8]-'0')*2 : (kto[8]-'0')*2-9);
#else

            pz=(kto[3]-'0')+(kto[5]-'0')+(kto[7]-'0');
            if(kto[4]<'5')pz+=(kto[4]-'0')*2; else pz+=(kto[4]-'0')*2-9;
            if(kto[6]<'5')pz+=(kto[6]-'0')*2; else pz+=(kto[6]-'0')*2-9;
            if(kto[8]<'5')pz+=(kto[8]-'0')*2; else pz+=(kto[8]-'0')*2-9;
#endif
            MOD_10_80;   /* pz%=10 */
         }
         else{
#ifdef __ALPHA
            pz = ((kto[0]<'5') ? (kto[0]-'0')*2 : (kto[0]-'0')*2-9)
               +  (kto[1]-'0')
               + ((kto[2]<'5') ? (kto[2]-'0')*2 : (kto[2]-'0')*2-9)
               +  (kto[3]-'0')
               + ((kto[4]<'5') ? (kto[4]-'0')*2 : (kto[4]-'0')*2-9)
               +  (kto[5]-'0')
               + ((kto[6]<'5') ? (kto[6]-'0')*2 : (kto[6]-'0')*2-9)
               +  (kto[7]-'0')
               + ((kto[8]<'5') ? (kto[8]-'0')*2 : (kto[8]-'0')*2-9);
#else

            pz=(kto[1]-'0')+(kto[3]-'0')+(kto[5]-'0')+(kto[7]-'0');
            if(kto[0]<'5')pz+=(kto[0]-'0')*2; else pz+=(kto[0]-'0')*2-9;
            if(kto[2]<'5')pz+=(kto[2]-'0')*2; else pz+=(kto[2]-'0')*2-9;
            if(kto[4]<'5')pz+=(kto[4]-'0')*2; else pz+=(kto[4]-'0')*2-9;
            if(kto[6]<'5')pz+=(kto[6]-'0')*2; else pz+=(kto[6]-'0')*2-9;
            if(kto[8]<'5')pz+=(kto[8]-'0')*2; else pz+=(kto[8]-'0')*2-9;
#endif
            MOD_10_80;   /* pz%=10 */
         }
         pz=pz%10;
         if(pz)pz=10-pz;
         CHECK_PZ10;

/*  Berechnung nach der Methode 42 +§§§4 */
/*
 * ######################################################################
 * #              Berechnung nach der Methode 42                        #
 * ######################################################################
 * # Modulus 11, Gewichtung 2, 3, 4, 5, 6, 7, 8, 9.                     #
 * # Die Kontonummer ist 10-stellig. Die einzelnen Stellen der          #
 * # Kontonummer werden von links nach rechts aufsteigend von           #
 * # 1 bis 10 durchnumeriert. Die Stelle 10 der Kontonummer ist         #
 * # per Definition die Prüfziffer.                                     #
 * # Es wird das Berechnungsverfahren 06 in modifizierter Form          #
 * # auf die Stellen 2 bis 9 angewendet. Die Gewichtung ist             #
 * # 2, 3, 4, 5, 6, 7, 8, 9. Die genannten Stellen werden von           #
 * # rechts nach links mit diesen Faktoren multipliziert. Die           #
 * # restliche Berechnung und mögliche Ergebnisse entsprechen           #
 * # dem Verfahren 06.                                                  #
 * ######################################################################
 */
      case 42:
#if DEBUG>0
         if(retvals){
            retvals->methode="42";
            retvals->pz_methode=42;
         }
#endif
         pz = (kto[1]-'0') * 9
            + (kto[2]-'0') * 8
            + (kto[3]-'0') * 7
            + (kto[4]-'0') * 6
            + (kto[5]-'0') * 5
            + (kto[6]-'0') * 4
            + (kto[7]-'0') * 3
            + (kto[8]-'0') * 2;

         MOD_11_352;   /* pz%=11 */
         if(pz<=1)
            pz=0;
         else
            pz=11-pz;
         CHECK_PZ10;

/*  Berechnung nach der Methode 43 +§§§4 */
/*
 * ######################################################################
 * #              Berechnung nach der Methode 43                        #
 * ######################################################################
 * # Modulus 10, Gewichtung 1, 2, 3, 4, 5, 6, 7, 8, 9.                  #
 * # Die Kontonummer ist 10-stellig. Die einzelnen Stellen der          #
 * # Kontonummer werden von links nach rechts aufsteigend von           #
 * # 1 bis 10 durchnumeriert. Die Stelle 10 der Kontonummer ist         #
 * # per Definition die Prüfziffer.                                     #
 * # Das Verfahren wird auf die Stellen 1 bis 9 angewendet.             #
 * # Die genannten Stellen werden von rechts nach links                 #
 * # mit den Gewichtungsfaktoren multipliziert. Die Summe der           #
 * # Produkte wird durch den Wert 10 dividiert. Der Rest der            #
 * # Division wird vom Divisor subtrahiert. Die Differenz               #
 * # ist die Prüfziffer. Ergibt die Berechnung eine Differenz           #
 * # von 10, lautet die Prüfziffer 0.                                   #
 * ######################################################################
 */
      case 43:
#if DEBUG>0
         if(retvals){
            retvals->methode="43";
            retvals->pz_methode=43;
         }
#endif
         pz = (kto[0]-'0') * 9
            + (kto[1]-'0') * 8
            + (kto[2]-'0') * 7
            + (kto[3]-'0') * 6
            + (kto[4]-'0') * 5
            + (kto[5]-'0') * 4
            + (kto[6]-'0') * 3
            + (kto[7]-'0') * 2
            + (kto[8]-'0');

         MOD_10_320;   /* pz%=10 */
         if(pz)pz=10-pz;
         CHECK_PZ10;

/*  Berechnung nach der Methode 44 +§§§4 */
/*
 * ######################################################################
 * #              Berechnung nach der Methode 44                        #
 * ######################################################################
 * # Modulus 11, Gewichtung 2, 4, 8, 5, A, 0, 0, 0, 0  (A = 10)         #
 * #                                                                    #
 * # Die Berechnung erfolgt wie bei Verfahren 33.                       #
 * # Stellennr.:    1 2 3 4 5 6 7 8 9 10                                #
 * # Kontonr.:      x x x x x x x x x P                                 #
 * # Gewichtung:    0 0 0 0 A 5 8 4 2    (A = 10)                       #
 * ######################################################################
 */
      case 44:
#if DEBUG>0
         if(retvals){
            retvals->methode="44";
            retvals->pz_methode=44;
         }
#endif
         pz = (kto[4]-'0') * 10
            + (kto[5]-'0') * 5
            + (kto[6]-'0') * 8
            + (kto[7]-'0') * 4
            + (kto[8]-'0') * 2;

         MOD_11_176;   /* pz%=11 */
         if(pz<=1)
            pz=0;
         else
            pz=11-pz;
         CHECK_PZ10;

/*  Berechnung nach der Methode 45 +§§§4 */
/*
 * ######################################################################
 * #              Berechnung nach der Methode 45                        #
 * ######################################################################
 * # Modulus 10, Gewichtung 2, 1, 2, 1, 2, 1, 2, 1, 2                   #
 * # Die Berechnung erfolgt wie bei Verfahren 00                        #
 * # Ausnahme:                                                          #
 * # Kontonummern, die an Stelle 1 (von links) eine 0 enthalten,        #
 * # und Kontonummern, die an Stelle 5 eine 1 enthalten,                #
 * # beinhalten keine Prüfziffer.                                       #
 * # Testkontonummern:                                                  #
 * # 3545343232, 4013410024                                             #
 * # Keine Prüfziffer enthalten:                                        #
 * # 0994681254, 0000012340 (da 1. Stelle = 0)                          #
 * # 1000199999, 0100114240 (da 5. Stelle = 1)                          #
 * ######################################################################
 */
      case 45:
#if DEBUG>0
         if(retvals){
            retvals->methode="45";
            retvals->pz_methode=45;
         }
#endif
         if(*kto=='0' || *(kto+4)=='1'){
#if DEBUG>0
            pz= *(kto+9)-'0';
#endif
            return OK_NO_CHK;
         }
#ifdef __ALPHA
         pz = ((kto[0]<'5') ? (kto[0]-'0')*2 : (kto[0]-'0')*2-9)
            +  (kto[1]-'0')
            + ((kto[2]<'5') ? (kto[2]-'0')*2 : (kto[2]-'0')*2-9)
            +  (kto[3]-'0')
            + ((kto[4]<'5') ? (kto[4]-'0')*2 : (kto[4]-'0')*2-9)
            +  (kto[5]-'0')
            + ((kto[6]<'5') ? (kto[6]-'0')*2 : (kto[6]-'0')*2-9)
            +  (kto[7]-'0')
            + ((kto[8]<'5') ? (kto[8]-'0')*2 : (kto[8]-'0')*2-9);
#else
         pz=(kto[1]-'0')+(kto[3]-'0')+(kto[5]-'0')+(kto[7]-'0');
         if(kto[0]<'5')pz+=(kto[0]-'0')*2; else pz+=(kto[0]-'0')*2-9;
         if(kto[2]<'5')pz+=(kto[2]-'0')*2; else pz+=(kto[2]-'0')*2-9;
         if(kto[4]<'5')pz+=(kto[4]-'0')*2; else pz+=(kto[4]-'0')*2-9;
         if(kto[6]<'5')pz+=(kto[6]-'0')*2; else pz+=(kto[6]-'0')*2-9;
         if(kto[8]<'5')pz+=(kto[8]-'0')*2; else pz+=(kto[8]-'0')*2-9;
#endif
         MOD_10_80;   /* pz%=10 */
         if(pz)pz=10-pz;
         CHECK_PZ10;

/*  Berechnung nach der Methode 46 +§§§4 */
/*
 * ######################################################################
 * #              Berechnung nach der Methode 46                        #
 * ######################################################################
 * # Modulus 11, Gewichtung 2, 3, 4, 5, 6.                              #
 * # Die Kontonummer ist 10-stellig. Die einzelnen Stellen der          #
 * # Kontonummer werden von links nach rechts aufsteigend von           #
 * # 1 bis 10 durchnumeriert. Die Stelle 8 der Kontonummer ist          #
 * # per Definition die Prüfziffer.                                     #
 * # Es wird das Berechnungsverfahren 06 in modifizierter Form          #
 * # auf die Stellen 3 bis 7 angewendet. Die Gewichtung ist             #
 * # 2, 3, 4, 5, 6. Die genannten Stellen werden von                    #
 * # rechts nach links mit diesen Faktoren multipliziert. Die           #
 * # restliche Berechnung und mögliche Ergebnisse entsprechen           #
 * # dem Verfahren 06.                                                  #
 * ######################################################################
 */
      case 46:
#if DEBUG>0
         if(retvals){
            retvals->methode="46";
            retvals->pz_methode=46;
         }
#endif
         pz = (kto[2]-'0') * 6
            + (kto[3]-'0') * 5
            + (kto[4]-'0') * 4
            + (kto[5]-'0') * 3
            + (kto[6]-'0') * 2;

         MOD_11_176;   /* pz%=11 */
         if(pz<=1)
            pz=0;
         else
            pz=11-pz;
         CHECK_PZ8;

/*  Berechnung nach der Methode 47 +§§§4 */
/*
 * ######################################################################
 * #              Berechnung nach der Methode 47                        #
 * ######################################################################
 * # Modulus 11, Gewichtung 2, 3, 4, 5, 6.                              #
 * # Die Kontonummer ist 10-stellig. Die einzelnen Stellen der          #
 * # Kontonummer werden von links nach rechts aufsteigend von           #
 * # 1 bis 10 durchnumeriert. Die Stelle 9 der Kontonummer ist          #
 * # per Definition die Prüfziffer.                                     #
 * # Es wird das Berechnungsverfahren 06 in modifizierter Form          #
 * # auf die Stellen 4 bis 8 angewendet. Die Gewichtung ist             #
 * # 2, 3, 4, 5, 6. Die genannten Stellen werden von                    #
 * # rechts nach links mit diesen Faktoren multipliziert. Die           #
 * # restliche Berechnung und mögliche Ergebnisse entsprechen           #
 * # dem Verfahren 06.                                                  #
 * ######################################################################
 */
      case 47:
#if DEBUG>0
         if(retvals){
            retvals->methode="47";
            retvals->pz_methode=47;
         }
#endif
         pz = (kto[3]-'0') * 6
            + (kto[4]-'0') * 5
            + (kto[5]-'0') * 4
            + (kto[6]-'0') * 3
            + (kto[7]-'0') * 2;

         MOD_11_176;   /* pz%=11 */
         if(pz<=1)
            pz=0;
         else
            pz=11-pz;
         CHECK_PZ9;

/*  Berechnung nach der Methode 48 +§§§4 */
/*
 * ######################################################################
 * #              Berechnung nach der Methode 48                        #
 * ######################################################################
 * # Modulus 11, Gewichtung 2, 3, 4, 5, 6, 7.                           #
 * # Die Kontonummer ist 10-stellig. Die einzelnen Stellen der          #
 * # Kontonummer werden von links nach rechts aufsteigend von           #
 * # 1 bis 10 durchnumeriert. Die Stelle 9 der Kontonummer ist          #
 * # per Definition die Prüfziffer.                                     #
 * # Es wird das Berechnungsverfahren 06 in modifizierter Form          #
 * # auf die Stellen 3 bis 8 angewendet. Die Gewichtung ist             #
 * # 2, 3, 4, 5, 6, 7. Die genannten Stellen werden von                 #
 * # rechts nach links mit diesen Faktoren multipliziert. Die           #
 * # restliche Berechnung und mögliche Ergebnisse entsprechen           #
 * # dem Verfahren 06.                                                  #
 * ######################################################################
 */
      case 48:
#if DEBUG>0
         if(retvals){
            retvals->methode="48";
            retvals->pz_methode=48;
         }
#endif
         pz = (kto[2]-'0') * 7
            + (kto[3]-'0') * 6
            + (kto[4]-'0') * 5
            + (kto[5]-'0') * 4
            + (kto[6]-'0') * 3
            + (kto[7]-'0') * 2;

         MOD_11_176;   /* pz%=11 */
         if(pz<=1)
            pz=0;
         else
            pz=11-pz;
         CHECK_PZ9;

/*  Berechnung nach der Methode 49 +§§§4 */
/*
 * ######################################################################
 * #              Berechnung nach der Methode 49                        #
 * ######################################################################
 * # Variante 1                                                         #
 * # Die Prüfzifferberechnung ist nach Kennziffer 00                    #
 * # durchzuführen. Führt die Berechnung nach Variante 1 zu             #
 * # einem Prüfzifferfehler, so ist die Berechnung nach                 #
 * # Variante 2 vorzunehmen.                                            #
 * #                                                                    #
 * # Variante 2                                                         #
 * # Die Prüfzifferberechnung ist nach Kennziffer 01                    #
 * # durchzuführen.                                                     #
 * ######################################################################
 */
      case 49:
#if DEBUG>0
      case 1049:
         if(retvals){
            retvals->methode="49a";
            retvals->pz_methode=1049;
         }
#endif
#ifdef __ALPHA
         pz = ((kto[0]<'5') ? (kto[0]-'0')*2 : (kto[0]-'0')*2-9)
            +  (kto[1]-'0')
            + ((kto[2]<'5') ? (kto[2]-'0')*2 : (kto[2]-'0')*2-9)
            +  (kto[3]-'0')
            + ((kto[4]<'5') ? (kto[4]-'0')*2 : (kto[4]-'0')*2-9)
            +  (kto[5]-'0')
            + ((kto[6]<'5') ? (kto[6]-'0')*2 : (kto[6]-'0')*2-9)
            +  (kto[7]-'0')
            + ((kto[8]<'5') ? (kto[8]-'0')*2 : (kto[8]-'0')*2-9);
#else
         pz=(kto[1]-'0')+(kto[3]-'0')+(kto[5]-'0')+(kto[7]-'0');
         if(kto[0]<'5')pz+=(kto[0]-'0')*2; else pz+=(kto[0]-'0')*2-9;
         if(kto[2]<'5')pz+=(kto[2]-'0')*2; else pz+=(kto[2]-'0')*2-9;
         if(kto[4]<'5')pz+=(kto[4]-'0')*2; else pz+=(kto[4]-'0')*2-9;
         if(kto[6]<'5')pz+=(kto[6]-'0')*2; else pz+=(kto[6]-'0')*2-9;
         if(kto[8]<'5')pz+=(kto[8]-'0')*2; else pz+=(kto[8]-'0')*2-9;
#endif
         MOD_10_80;   /* pz%=10 */
         if(pz)pz=10-pz;
         CHECK_PZX10;

#if DEBUG>0
      case 2049:
         if(retvals){
            retvals->methode="49b";
            retvals->pz_methode=2049;
         }
#endif
         pz = (kto[0]-'0')
            + (kto[1]-'0') * 7
            + (kto[2]-'0') * 3
            + (kto[3]-'0')
            + (kto[4]-'0') * 7
            + (kto[5]-'0') * 3
            + (kto[6]-'0')
            + (kto[7]-'0') * 7
            + (kto[8]-'0') * 3;

         MOD_10_160;   /* pz%=10 */
         if(pz)pz=10-pz;
         CHECK_PZ10;

/* Berechnungsmethoden 50 bis 59 +§§§3
   Berechnung nach der Methode 50 +§§§4 */
/*
 * ######################################################################
 * #              Berechnung nach der Methode 50                        #
 * ######################################################################
 * # Modulus 11, Gewichtung 2, 3, 4, 5, 6, 7.                           #
 * # Die Kontonummer ist 10-stellig. Die einzelnen Stellen der          #
 * # Kontonummer werden von links nach rechts aufsteigend von           #
 * # 1 bis 10 durchnumeriert. Die Stelle 7 der Kontonummer ist          #
 * # per Definition die Prüfziffer.                                     #
 * # Es wird das Berechnungsverfahren 06 in modifizierter Form          #
 * # auf die Stellen 1 bis 6 angewendet. Die Gewichtung ist             #
 * # 2, 3, 4, 5, 6, 7. Die genannten Stellen werden von                 #
 * # rechts nach links mit diesen Faktoren multipliziert. Die           #
 * # restliche Berechnung und mögliche Ergebnisse entsprechen           #
 * # dem Verfahren 06.                                                  #
 * # Ergibt die erste Berechnung einen Prüfziffernfehler, wird          #
 * # empfohlen, die Prüfziffernberechnung ein zweites Mal durch-        #
 * # zuführen und dabei die "gedachte" Unternummer 000 an die           #
 * # Stellen 8 bis 10 zu setzen und die vorhandene Kontonummer          #
 * # vorher um drei Stellen nach links zu verschieben                   #
 * ######################################################################
 */
      case 50:
#if DEBUG>0
      case 1050:
         if(retvals){
            retvals->methode="50a";
            retvals->pz_methode=1050;
         }
#endif
         pz = (kto[0]-'0') * 7
            + (kto[1]-'0') * 6
            + (kto[2]-'0') * 5
            + (kto[3]-'0') * 4
            + (kto[4]-'0') * 3
            + (kto[5]-'0') * 2;

         MOD_11_176;   /* pz%=11 */
         if(pz<=1)
            pz=0;
         else
            pz=11-pz;
         CHECK_PZX7;

#if DEBUG>0
      case 2050:
         if(retvals){
            retvals->methode="50b";
            retvals->pz_methode=2050;
         }
#endif
            /* es ist eine reale Kontonummer bekannt, bei der rechts nur eine
             * Null weggelassen wurde; daher wird die Berechnung für die
             * Methode 50b leicht modifiziert, so daß eine, zwei oder drei
             * Stellen der Unterkontonummer 000 weggelassen werden können.
             */
         if(kto[0]=='0' && kto[1]=='0' && kto[2]=='0'){
            pz = (kto[3]-'0') * 7
               + (kto[4]-'0') * 6
               + (kto[5]-'0') * 5
               + (kto[6]-'0') * 4
               + (kto[7]-'0') * 3
               + (kto[8]-'0') * 2;

            MOD_11_176;   /* pz%=11 */
            if(pz<=1)
               pz=0;
            else
               pz=11-pz;
            CHECK_PZ10;
         }
         else if(kto[0]=='0' && kto[1]=='0' && kto[9]=='0'){
            pz = (kto[2]-'0') * 7
               + (kto[3]-'0') * 6
               + (kto[4]-'0') * 5
               + (kto[5]-'0') * 4
               + (kto[6]-'0') * 3
               + (kto[7]-'0') * 2;

            MOD_11_176;   /* pz%=11 */
            if(pz<=1)
               pz=0;
            else
               pz=11-pz;
            CHECK_PZ9;
         }
         else if(kto[0]=='0' && kto[8]=='0' && kto[9]=='0'){
            pz = (kto[1]-'0') * 7
               + (kto[2]-'0') * 6
               + (kto[3]-'0') * 5
               + (kto[4]-'0') * 4
               + (kto[5]-'0') * 3
               + (kto[6]-'0') * 2;

            MOD_11_176;   /* pz%=11 */
            if(pz<=1)
               pz=0;
            else
               pz=11-pz;
            CHECK_PZ8;
         }
#if DEBUG>0
         else{   /* bei DEBUG wurde evl. direkt die Methode angesprungen; daher mit 50a testen */
            if(retvals){
               retvals->methode="50a";
               retvals->pz_methode=1050;
            }
            pz = (kto[0]-'0') * 7
               + (kto[1]-'0') * 6
               + (kto[2]-'0') * 5
               + (kto[3]-'0') * 4
               + (kto[4]-'0') * 3
               + (kto[5]-'0') * 2;

            MOD_11_176;   /* pz%=11 */
            if(pz<=1)
               pz=0;
            else
               pz=11-pz;
            CHECK_PZ7;
         }
#endif
         return FALSE;

/*  Berechnung nach der Methode 51 +§§§4 */
/*
 * ######################################################################
 * #              Berechnung nach der Methode 51 (geändert 6.9.04)      #
 * ######################################################################
 * # Die Kontonummer ist immer 10-stellig. Die für die Berechnung       #
 * # relevante Kundennummer (K) befindet sich bei der Methode A         #
 * # in den Stellen 4 bis 9 der Kontonummer und bei den Methoden        #
 * # B + C in den Stellen 5 bis 9, die Prüfziffer in Stelle 10          #
 * # der Kontonummer.                                                   #
 * #                                                                    #
 * # Methode A:                                                         #
 * # Modulus 11, Gewichtung 2, 3, 4, 5, 6, 7                            #
 * # Die Berechnung und mögliche Ergebnisse entsprechen dem             #
 * # Verfahren 06.                                                      #
 * # Stellennr.:    1 2 3 4 5 6 7 8 9 A (A = 10)                        #
 * # Kontonr.:      x x x K K K K K K P                                 #
 * # Gewichtung:          7 6 5 4 3 2                                   #
 * #                                                                    #
 * # Ergibt die Berechnung der Prüfziffer nach der Methode A            #
 * # einen Prüfzifferfehler, ist eine weitere Berechnung mit der        #
 * # Methode B vorzunehmen.                                             #
 * #                                                                    #
 * # Methode B:                                                         #
 * # Modulus     11, Gewichtung 2, 3, 4, 5, 6                           #
 * # Die Berechnung und mögliche Ergebnisse entsprechen dem             #
 * # Verfahren 33.                                                      #
 * # Stellennr.:    1 2 3 4 5 6 7 8 9 A (A = 10)                        #
 * # Kontonr.:      x x x x K K K K K P                                 #
 * # Gewichtung:            6 5 4 3 2                                   #
 * #                                                                    #
 * # Ergibt auch die Berechnung der Prüfziffer nach Methode B           #
 * # einen Prüfzifferfehler, ist eine weitere Berechnung mit der        #
 * # Methode C vorzunehmen.                                             #
 * #                                                                    #
 * # Methode C:                                                         #
 * # Kontonummern, die bis zur Methode C gelangen und in der 10.        #
 * # Stelle eine 7, 8 oder 9 haben, sind ungültig.                      #
 * # Modulus 7, Gewichtung 2, 3, 4, 5, 6                                #
 * # Das Berechnungsverfahren entspricht Methode B. Die Summe der       #
 * # Produkte ist jedoch durch 7 zu dividieren. Der verbleibende        #
 * # Rest wird vom Divisor (7) subtrahiert. Das Ergebnis ist die        #
 * # Prüfziffer. Verbleibt kein Rest, ist die Prüfziffer 0.             #
 * #                                                                    #
 * # Ausnahme:                                                          #
 * # Ist nach linksbündiger Auffüllung mit Nullen auf 10 Stellen die    #
 * # 3. Stelle der Kontonummer = 9 (Sachkonten), so erfolgt die         #
 * # Berechnung wie folgt:                                              #
 * #                                                                    #
 * # Variante 1 zur Ausnahme                                            #
 * # Modulus 11, Gewichtung 2, 3, 4, 5, 6, 7, 8                         #
 * # Die für die Berechnung relevanten Stellen 3 bis 9 werden von       #
 * # rechts nach links mit den Ziffern 2, 3, 4, 5, 6, 7, 8              #
 * # multipliziert. Die Produkte werden addiert. Die Summe ist durch 11 #
 * # zu dividieren. Der verbleibende Rest wird vom Divisor (11)         #
 * # subtrahiert. Das Ergebnis ist die Prüfziffer. Ergibt sich als Rest #
 * # 1 oder 0, ist die Prüfziffer 0.                                    #
 * #                                                                    #
 * # Stellennr.:   1 2 3 4 5 6 7 8 9 A (A=10)                           #
 * # Kontonr.;     x x 9 x x x x x x P                                  #
 * # Gewichtung:       8 7 6 5 4 3 2                                    #
 * #                                                                    #
 * # Führt die Variante 1 zur Ausnahme zu einem Prüfzifferfehler, ist   #
 * # eine weitere Berechnung nach der Variante 2 vorzunehmen.           #
 * #                                                                    #
 * # Variante 2 zur Ausnahme                                            #
 * # Modulus 11, Gewichtung 2, 3, 4, 5, 6, 7, 8, 9, 10                  #
 * # Berechnung und Ergebnisse entsprechen der Variante 1 zur Ausnahme. #
 * ######################################################################
 */
      case 51:
         if(*(kto+2)=='9'){   /* Ausnahme */

            /* Variante 1 */
#if DEBUG>0
      case 4051:
         if(retvals){
            retvals->methode="51d";
            retvals->pz_methode=4051;
         }
#endif
            pz =         9 * 8
            + (kto[3]-'0') * 7
            + (kto[4]-'0') * 6
            + (kto[5]-'0') * 5
            + (kto[6]-'0') * 4
            + (kto[7]-'0') * 3
            + (kto[8]-'0') * 2;

            MOD_11_176;   /* pz%=11 */
            if(pz<=1)
               pz=0;
            else
               pz=11-pz;
            CHECK_PZX10;

            /* Variante 2 */
#if DEBUG>0
      case 5051:
         if(retvals){
            retvals->methode="51e";
            retvals->pz_methode=5051;
         }
#endif
               pz = (kto[0]-'0') * 10
               + (kto[1]-'0') * 9
               +            9 * 8
               + (kto[3]-'0') * 7
               + (kto[4]-'0') * 6
               + (kto[5]-'0') * 5
               + (kto[6]-'0') * 4
               + (kto[7]-'0') * 3
               + (kto[8]-'0') * 2;

               MOD_11_352;   /* pz%=11 */
               if(pz<=1)
                  pz=0;
               else
                  pz=11-pz;
               CHECK_PZ10;
            }

            /* Methode A */
#if DEBUG>0
      case 1051:
         if(retvals){
            retvals->methode="51a";
            retvals->pz_methode=1051;
         }
#endif
         pz = (kto[3]-'0') * 7
            + (kto[4]-'0') * 6
            + (kto[5]-'0') * 5
            + (kto[6]-'0') * 4
            + (kto[7]-'0') * 3
            + (kto[8]-'0') * 2;

         MOD_11_176;   /* pz%=11 */
         if(pz<=1)
            pz=0;
         else
            pz=11-pz;
         CHECK_PZX10;

            /* Methode B */
#if DEBUG>0
      case 2051:
         if(retvals){
            retvals->methode="51b";
            retvals->pz_methode=2051;
         }
#endif
         pz = (kto[4]-'0') * 6
            + (kto[5]-'0') * 5
            + (kto[6]-'0') * 4
            + (kto[7]-'0') * 3
            + (kto[8]-'0') * 2;

         MOD_11_176;   /* pz%=11 */
         if(pz<=1)
            pz=0;
         else
            pz=11-pz;
         CHECK_PZX10;

            /* Methode C */
#if DEBUG>0
      case 3051:
         if(retvals){
            retvals->methode="51c";
            retvals->pz_methode=3051;
         }
#endif
         pz = (kto[4]-'0') * 6
            + (kto[5]-'0') * 5
            + (kto[6]-'0') * 4
            + (kto[7]-'0') * 3
            + (kto[8]-'0') * 2;

         MOD_7_112;   /* pz%=7 */
         if(pz)pz=7-pz;
         CHECK_PZ10;


/*  Berechnung nach der Methode 52 +§§§4 */
/*
 * ######################################################################
 * #              Berechnung nach der Methode 52                        #
 * ######################################################################
 * # Modulus 11, Gewichtung 2, 4, 8, 5, 10, 9, 7, 3, 6, 1, 2, 4.        #
 * # Zur Berechnung der Prüfziffer muß zunächst aus der angegebenen     #
 * # achtstelligen Kontonummer die zugehörige Kontonummer des ESER-     #
 * # Altsystems (maximal 12stellig) ermittelt werden. Die einzelnen     #
 * # Stellen dieser Alt-Kontonummer sind von rechts nach links mit      #
 * # den Ziffern 2, 4, 8, 5, 10, 9, 7, 3, 6, 1, 2, 4 zu multipli-       #
 * # zieren. Dabei ist für die Prüfziffer, die sich immer an der        #
 * # 6. Stelle von links der Alt-Kontonummer befindet, 0 zu setzen.     #
 * # Die jeweiligen Produkte werden addiert und die Summe durch 11      #
 * # dividiert. Zum Divisionsrest (ggf. auch 0) ist das Gewicht         #
 * # oder ein Vielfaches des Gewichtes über der Prüfziffer zu           #
 * # addieren. Die Summe wird durch 11 dividiert; der Divisionsrest     #
 * # muß 10 lauten. Die Prüfziffer ist der verwendete Faktor des        #
 * # Gewichtes. Kann bei der Division kein Rest 10 erreicht werden,     #
 * # ist die Konto-Nr. nicht verwendbar.                                #
 * # Bildung der Konto-Nr. des ESER-Altsystems aus angegebener          #
 * # Bankleitzahl und Konto-Nr.: XXX5XXXX XPXXXXXX (P=Prüfziffer)       #
 * # Kontonummer des Altsystems: XXXX-XP-XXXXX (XXXXX = variable        #
 * # Länge, da evtl.vorlaufende Nullen eliminiert werden).              #
 * # Bei 10stelligen, mit 9 beginnenden Kontonummern ist die            #
 * # Prüfziffer nach Kennziffer 20 zu berechnen.                        #
 * ######################################################################
 */
      case 52:

            /* Berechnung nach Methode 20 */
#if DEBUG>0
      case 1052:
         if(retvals){
            retvals->methode="52a";
            retvals->pz_methode=1052;
         }
#endif
         if(*kto=='9'){
            pz = (kto[0]-'0') * 3
               + (kto[1]-'0') * 9
               + (kto[2]-'0') * 8
               + (kto[3]-'0') * 7
               + (kto[4]-'0') * 6
               + (kto[5]-'0') * 5
               + (kto[6]-'0') * 4
               + (kto[7]-'0') * 3
               + (kto[8]-'0') * 2;

            MOD_11_352;   /* pz%=11 */
            if(pz<=1)
               pz=0;
            else
               pz=11-pz;
            CHECK_PZ10;
         }

               /* nur Prüfziffer angegeben; Test-BLZ einsetzen */
#if DEBUG>0
      case 2052:
         if(retvals){
            retvals->methode="52b";
            retvals->pz_methode=2052;
         }
#endif
         if(!x_blz){
            ok=OK_TEST_BLZ_USED;
            x_blz="13051172";
         }
         else
            ok=OK;

            /* Generieren der Konto-Nr. des ESER-Altsystems */
         for(ptr=kto;*ptr=='0';ptr++);
         if(ptr>kto+2)return INVALID_KTO;
         kto_alt[0]=x_blz[4];
         kto_alt[1]=x_blz[5];
         kto_alt[2]=x_blz[6];
         kto_alt[3]=x_blz[7];
         kto_alt[4]= *ptr++;
         kto_alt[5]= *ptr++;
#if DEBUG>0
         if(retvals){
            retvals->pz_pos=(ptr-kto)+1;
            retvals->pz=kto_alt[5]-'0';
         }
#endif
         while(*ptr=='0' && *ptr)ptr++;
         for(dptr=kto_alt+6;(*dptr= *ptr++);dptr++);
            /* die Konto-Nr. des ESER-Altsystems darf maximal 12 Stellen haben */
         if((dptr-kto_alt)>12)return INVALID_KTO;
         p1=kto_alt[5];   /* Prüfziffer */
         kto_alt[5]='0';
         for(pz=0,ptr=dptr-1,i=0;ptr>=kto_alt;ptr--,i++)pz+=(*ptr-'0')*w52[i];
         kto_alt[5]=p1;
         pz=pz%11;
         p1=w52[i-6];

            /* passenden Faktor suchen */
         tmp=pz;
         for(i=0;i<10;i++){
            pz=tmp+p1*i;
            MOD_11_88;
            if(pz==10)break;
         }
         pz=i; /* Prüfziffer ist der verwendete Faktor des Gewichtes */
         INVALID_PZ10;
         if(*(kto_alt+5)-'0'==pz)
            return ok;
         else
            return FALSE;

/*  Berechnung nach der Methode 53 +§§§4 */
/*
 * ######################################################################
 * #              Berechnung nach der Methode 53                        #
 * ######################################################################
 * # Analog Kennziffer 52, jedoch für neunstellige Kontonummern.        #
 * # Bildung der Kontonummern des ESER-Altsystems aus angegebener       #
 * # Bankleitzahl und angegebener neunstelliger Kontonummer:            #
 * # XXX5XXXX XTPXXXXXX (P=Prüfziffer)                                  #
 * # Kontonummer des Altsystems: XXTX-XP-XXXXX (XXXXX = variable        #
 * # Länge, da evtl.vorlaufende Nullen eliminiert werden).              #
 * # Die Ziffer T ersetzt die 3. Stelle von links der nach              #
 * # Kennziffer 52 gebildeten Kontonummer des ESER-Altsystems.          #
 * # Bei der Bildung der Kontonummer des ESER-Altsystems wird die       #
 * # Ziffer T nicht in den Kundennummernteil (7.-12. Stelle der         #
 * # Kontonummer) übernommen.                                           #
 * # Bei 10stelligen, mit 9 beginnenden Kontonummern ist die            #
 * # Prüfziffer nach Kennziffer 20 zu berechnen.                        #
 * ######################################################################
 */
      case 53:

            /* Berechnung nach Methode 20 */
         if(*kto=='9'){
#if DEBUG>0
      case 1053:
         if(retvals){
            retvals->methode="53a";
            retvals->pz_methode=1053;
         }
#endif
         pz = (kto[0]-'0') * 3
            + (kto[1]-'0') * 9
            + (kto[2]-'0') * 8
            + (kto[3]-'0') * 7
            + (kto[4]-'0') * 6
            + (kto[5]-'0') * 5
            + (kto[6]-'0') * 4
            + (kto[7]-'0') * 3
            + (kto[8]-'0') * 2;

         MOD_11_352;   /* pz%=11 */
            if(pz<=1)
               pz=0;
            else
               pz=11-pz;
            CHECK_PZ10;
         }

               /* nur Prüfziffer angegeben; Test-BLZ einsetzen */
#if DEBUG>0
      case 2053:
         if(retvals){
            retvals->methode="53b";
            retvals->pz_methode=2053;
         }
#endif
         if(!x_blz){
            ok=OK_TEST_BLZ_USED;
            x_blz="16052072";
         }
         else
            ok=OK;

            /* Generieren der Konto-Nr. des ESER-Altsystems */
         for(ptr=kto;*ptr=='0';ptr++);
         if(*kto!='0' || *(kto+1)=='0'){  /* Kto-Nr. muß neunstellig sein */
#if DEBUG>0
            if(retvals)retvals->pz= -2;
#endif
            return INVALID_KTO;
         }
         kto_alt[0]=x_blz[4];
         kto_alt[1]=x_blz[5];
         kto_alt[2]=kto[2];         /* T-Ziffer */
         kto_alt[3]=x_blz[7];
         kto_alt[4]=kto[1];
         kto_alt[5]=kto[3];
         for(ptr=kto+4;*ptr=='0' && *ptr;ptr++);
         for(dptr=kto_alt+6;(*dptr= *ptr++);dptr++);
         kto=kto_alt;
         p1=kto_alt[5];   /* Prüfziffer merken */
         kto_alt[5]='0';
         for(pz=0,ptr=kto_alt+strlen(kto_alt)-1,i=0;ptr>=kto_alt;ptr--,i++)
            pz+=(*ptr-'0')*w52[i];
         kto_alt[5]=p1;   /* Prüfziffer zurückschreiben */
         pz=pz%11;
         p1=w52[i-6];

            /* passenden Faktor suchen */
         tmp=pz;
         for(i=0;i<10;i++){
            pz=tmp+p1*i;
            MOD_11_88;
            if(pz==10)break;
         }
         pz=i; /* Prüfziffer ist der verwendete Faktor des Gewichtes */
#if DEBUG>0
         if(retvals)retvals->pz=pz; 
#endif
         INVALID_PZ10;
         if(*(kto+5)-'0'==pz)
            return ok;
         else
            return FALSE;

/*  Berechnung nach der Methode 54 +§§§4 */
/*
 * ######################################################################
 * #              Berechnung nach der Methode 54                        #
 * ######################################################################
 * # Modulus 11, Gewichtung 2, 3, 4, 5, 6, 7, 2                         #
 * # Die Kontonummer ist 10-stellig, wobei die Stellen 1 u. 2           #
 * # generell mit 49 belegt sind. Die einzelnen Stellen der             #
 * # Kontonummer sind von rechts nach links mit den Ziffern 2, 3,       #
 * # 4, 5, 6, 7, 2 zu multiplizieren. Die jeweiligen Produkte werden    #
 * # addiert. Die Summe ist durch 11 zu dividieren. Der verbleibende    #
 * # Rest wird vom Divisor (11) subtrahiert. Das Ergebnis ist die       #
 * # Prüfziffer. Ergibt sich als Rest 0 oder 1, ist die Prüfziffer      #
 * # zweistellig und kann nicht verwendet werden. Die Kontonummer       #
 * # ist dann nicht verwendbar.                                         #
 * ######################################################################
 */
      case 54:
#if DEBUG>0
         if(retvals){
            retvals->methode="54";
            retvals->pz_methode=54;
         }
#endif
         if(*kto!='4' && *(kto+1)!='9')return INVALID_KTO;
         pz = (kto[2]-'0') * 2
            + (kto[3]-'0') * 7
            + (kto[4]-'0') * 6
            + (kto[5]-'0') * 5
            + (kto[6]-'0') * 4
            + (kto[7]-'0') * 3
            + (kto[8]-'0') * 2;

         MOD_11_176;   /* pz%=11 */
         pz=11-pz;
         if(pz>9)return INVALID_KTO;
         CHECK_PZ10;

/*  Berechnung nach der Methode 55 +§§§4 */
/*
 * ######################################################################
 * #              Berechnung nach der Methode 55                        #
 * ######################################################################
 * # Modulus 11, Gewichtung 2, 3, 4, 5, 6, 7, 8, 7, 8 (modifiziert).    #
 * # Die Berechnung erfolgt wie bei Verfahren 06.                       #
 * # Die einzelnen Stellen der Kontonummer sind von rechts nach         #
 * # links mit den Ziffern 2, 3, 4, 5, 6, 7, 8, 7, 8 zu                 #
 * # multiplizieren. Die jeweiligen Produkte werden addiert.            #
 * # Die Summe ist durch 11 zu dividieren. Der verbleibende Rest        #
 * # wird vom Divisor (11) subtrahiert. Das Ergebnis ist die            #
 * # Prüfziffer. Verbleibt nach der Division durch 11 kein Rest,        #
 * # ist die Prüfziffer 0. Ergibt sich als Rest 1, entsteht bei         #
 * # der Subtraktion 11 - 1 = 10. Das Rechenergebnis ist                #
 * # nicht verwendbar und muß auf eine Stelle reduziert werden.         #
 * # Die linke Seite wird eliminiert, und nur die rechte Stelle         #
 * # (Null) findet als Prüfziffer Verwendung.                           #
 * ######################################################################
 */
      case 55:
#if DEBUG>0
         if(retvals){
            retvals->methode="55";
            retvals->pz_methode=55;
         }
#endif
         pz = (kto[0]-'0') * 8
            + (kto[1]-'0') * 7
            + (kto[2]-'0') * 8
            + (kto[3]-'0') * 7
            + (kto[4]-'0') * 6
            + (kto[5]-'0') * 5
            + (kto[6]-'0') * 4
            + (kto[7]-'0') * 3
            + (kto[8]-'0') * 2;

         MOD_11_352;   /* pz%=11 */
         if(pz<=1)
            pz=0;
         else
            pz=11-pz;
         CHECK_PZ10;

/*  Berechnung nach der Methode 56 +§§§4 */
/*
 * ######################################################################
 * #              Berechnung nach der Methode 56                        #
 * ######################################################################
 * # Modulus 11, Gewichtung 2, 3, 4, 5, 6, 7, 2, 3, 4.                  #
 * # Prüfziffer ist die letzte Stelle der Kontonummer.                  #
 * # Von rechts beginnend werden die einzelnen Ziffern der              #
 * # Kontonummer mit den Gewichten multipliziert. Die Produkte der      #
 * # Multiplikation werden addiert und diese Summe durch 11             #
 * # dividiert. Der Rest wird von 11 abgezogen, das Ergebnis ist        #
 * # die Prüfziffer, die an die Kontonummer angehängt wird.             #
 * # 1. Bei dem Ergebnis 10 oder 11 ist die Kontonummer ungültig.       #
 * # 2. Beginnt eine zehnstellige Kontonummer mit 9, so wird beim       #
 * # Ergebnis 10 die Prüfziffer 7 und beim Ergebnis 11 die              #
 * # Prüfziffer 8 gesetzt.                                              #
 * ######################################################################
 */
      case 56:
#if DEBUG>0
         if(retvals){
            retvals->methode="56";
            retvals->pz_methode=56;
         }
#endif
         pz = (kto[0]-'0') * 4
            + (kto[1]-'0') * 3
            + (kto[2]-'0') * 2
            + (kto[3]-'0') * 7
            + (kto[4]-'0') * 6
            + (kto[5]-'0') * 5
            + (kto[6]-'0') * 4
            + (kto[7]-'0') * 3
            + (kto[8]-'0') * 2;

         MOD_11_176;   /* pz%=11 */
         pz=11-pz;
         if(pz>9){
            if(*kto=='9'){
               if(pz==10)
                  pz=7;
               else
                  pz=8;
            }
            else
               return INVALID_KTO;
         }
         CHECK_PZ10;

/*  Berechnung nach der Methode 57 +§§§4 */
/*
 * ######################################################################
 * #    Berechnung nach der Methode 57 (geändert zum 04.12.2006)        #
 * ######################################################################
 * # Die Kontonummer ist einschließlich der Prüfziffer 10-stellig,      #
 * # ggf. ist die Kontonummer für die Prüfzifferberechnung durch        #
 * # linksbündige Auffüllung mit Nullen 10-stellig darzustellen. Die    #
 * # Berechnung der Prüfziffer und die möglichen Ergebnisse richten     #
 * # sich nach dem jeweils bei der entsprechenden Variante angegebenen  #
 * # Kontonummernkreis. Führt die Berechnung der Prüfziffer nach der    #
 * # vorgegebenen Variante zu einem Prüfzifferfehler, so ist die        #
 * # Kontonummer ungültig. Kontonummern, die mit 00 beginnen sind       #
 * # immer als falsch zu bewerten.                                      #
 * #                                                                    #
 * # Variante 1:                                                        #
 * # Modulus 10, Gewichtung 1, 2, 1, 2, 1, 2, 1, 2, 1                   #
 * # Anzuwenden ist dieses Verfahren für Kontonummern, die mit den      #
 * # folgenden Zahlen beginnen:                                         #
 * #                                                                    #
 * # 51, 55, 61, 64, 65, 66, 70, 73, 75 bis 82, 88, 94 und 95           #
 * #                                                                    #
 * # Die Stellen 1 bis 9 der Kontonummer sind von                       #
 * # links beginnend mit den Gewichten zu multiplizieren. Die 10.       #
 * # Stelle ist die Prüfziffer. Die Berechnung und mögliche Ergebnisse  #
 * # entsprechen der Methode 00.                                        #
 * #                                                                    #
 * # Ausnahme: Kontonummern, die mit den Zahlen 777777 oder 888888      #
 * # beginnen sind immer als richtig (= Methode 09; keine Prüfziffer-   #
 * # berechnung) zu bewerten.                                           #
 * #                                                                    #
 * #                                                                    #
 * # Variante 2:                                                        #
 * # Modulus 10, Gewichtung 1, 2, 1, 2, 1, 2, 1, 2, 1                   #
 * # Anzuwenden ist dieses Verfahren für Kontonummern, die mit den      #
 * # folgenden Zahlen beginnen:                                         #
 * #                                                                    #
 * # 32 bis 39, 41 bis 49, 52, 53, 54, 56 bis 60, 62, 63, 67, 68, 69,   #
 * # 71, 72, 74, 83 bis 87, 89, 90, 92, 93, 96, 97 und 98               #
 * #                                                                    #
 * # Die Stellen 1, 2, 4, 5, 6, 7, 8, 9 und 10 der Kontonummer sind     #
 * # von links beginnend mit den Gewichten zu multiplizieren. Die 3.    #
 * # Stelle ist die Prüfziffer. Die Berechnung und mögliche Ergebnisse  #
 * # entsprechen der Methode 00.                                        #
 * #                                                                    #
 * # Variante 3:                                                        #
 * # Für die Kontonummern, die mit den folgenden Zahlen beginnen gilt   #
 * # die Methode 09 (keine Prüfzifferberechnung):                       #
 * # 40, 50, 91 und 99                                                  #
 * #                                                                    #
 * # Variante 4:                                                        #
 * # Kontonummern die mit 01 bis 31 beginnen haben an der dritten bis   #
 * # vierten Stelle immer einen Wert zwischen 01 und 12 und an der      #
 * # siebten bis neunten Stelle immer einen Wert kleiner 500.           #
 * #                                                                    #
 * # Ausnahme: Die Kontonummer 0185125434 ist als richtig zu            #
 * # bewerten.                                                          #
 * ######################################################################
 */
      case 57:
#if DEBUG>0
      case 1057:  /* die Untermethoden werden in einem eigenen switch abgearbeitet, daher alle hier zusammen */
      case 2057:
      case 3057:
      case 4057:
         if(retvals){
            retvals->methode="57";
            retvals->pz_methode=57;
         }
#endif
            /* erstmal die Sonderfälle abhaken */
         if(!strncmp(kto,"777777",6) || !strncmp(kto,"888888",6)){
#if DEBUG>0
            if(retvals){
               retvals->methode="57a";
               retvals->pz_methode=1057;
            }
#endif
            return OK_NO_CHK;
         }
         if(!strcmp(kto,"0185125434")){
#if DEBUG>0
            if(retvals){
               retvals->methode="57d";
               retvals->pz_methode=4057;
            }
#endif
            return OK_NO_CHK;
         }

         tmp=(kto[0]-'0')*10+kto[1]-'0';  /* die ersten beiden Stellen als Integer holen */
         switch(tmp){
            case 51:
            case 55:
            case 61:
            case 64:
            case 65:
            case 66:
            case 70:
            case 73: 
            case 75:
            case 76:
            case 77:
            case 78:
            case 79:
            case 80:
            case 81:
            case 82: 
            case 88: 
            case 94:
            case 95:  /* Variante 1 */
#if DEBUG>0
               if(retvals){
                  retvals->methode="57a";
                  retvals->pz_methode=1057;
               }
#endif
#ifdef __ALPHA
               pz =  (kto[0]-'0')
                  + ((kto[1]<'5') ? (kto[1]-'0')*2 : (kto[1]-'0')*2-9)
                  +  (kto[2]-'0')
                  + ((kto[3]<'5') ? (kto[3]-'0')*2 : (kto[3]-'0')*2-9)
                  +  (kto[4]-'0')
                  + ((kto[5]<'5') ? (kto[5]-'0')*2 : (kto[5]-'0')*2-9)
                  +  (kto[6]-'0')
                  + ((kto[7]<'5') ? (kto[7]-'0')*2 : (kto[7]-'0')*2-9)
                  +  (kto[8]-'0');
#else
               pz=(kto[0]-'0')+(kto[2]-'0')+(kto[4]-'0')+(kto[6]-'0')+(kto[8]-'0');
               if(kto[1]<'5')pz+=(kto[1]-'0')*2; else pz+=(kto[1]-'0')*2-9;
               if(kto[3]<'5')pz+=(kto[3]-'0')*2; else pz+=(kto[3]-'0')*2-9;
               if(kto[5]<'5')pz+=(kto[5]-'0')*2; else pz+=(kto[5]-'0')*2-9;
               if(kto[7]<'5')pz+=(kto[7]-'0')*2; else pz+=(kto[7]-'0')*2-9;
#endif
               MOD_10_80;   /* pz%=10 */
               if(pz)pz=10-pz;
               CHECK_PZ10;
               break;

            case 32:
            case 33:
            case 34:
            case 35:
            case 36:
            case 37:
            case 38:
            case 39:
            case 41:
            case 42:
            case 43:
            case 44:
            case 45:
            case 46:
            case 47:
            case 48:
            case 49:
            case 52:
            case 53:
            case 54:
            case 56:
            case 57:
            case 58:
            case 59:
            case 60:
            case 62:
            case 63:
            case 67:
            case 68:
            case 69:
            case 71:
            case 72:
            case 74:
            case 83:
            case 84:
            case 85:
            case 86:
            case 87:
            case 89:
            case 90:
            case 92:
            case 93:
            case 96:
            case 97:
            case 98:  /* Variante 2 */
#if DEBUG>0
               if(retvals){
                  retvals->methode="57b";
                  retvals->pz_methode=2057;
               }
#endif
#ifdef __ALPHA
               pz =  (kto[0]-'0')
                  + ((kto[1]<'5') ? (kto[1]-'0')*2 : (kto[1]-'0')*2-9)
                  +  (kto[3]-'0')
                  + ((kto[4]<'5') ? (kto[4]-'0')*2 : (kto[4]-'0')*2-9)
                  +  (kto[5]-'0')
                  + ((kto[6]<'5') ? (kto[6]-'0')*2 : (kto[6]-'0')*2-9)
                  +  (kto[7]-'0')
                  + ((kto[8]<'5') ? (kto[8]-'0')*2 : (kto[8]-'0')*2-9)
                  +  (kto[9]-'0');
#else
               pz=(kto[0]-'0')+(kto[3]-'0')+(kto[5]-'0')+(kto[7]-'0')+(kto[9]-'0');
               if(kto[1]<'5')pz+=(kto[1]-'0')*2; else pz+=(kto[1]-'0')*2-9;
               if(kto[4]<'5')pz+=(kto[4]-'0')*2; else pz+=(kto[4]-'0')*2-9;
               if(kto[6]<'5')pz+=(kto[6]-'0')*2; else pz+=(kto[6]-'0')*2-9;
               if(kto[8]<'5')pz+=(kto[8]-'0')*2; else pz+=(kto[8]-'0')*2-9;
#endif
               MOD_10_80;   /* pz%=10 */
               if(pz)pz=10-pz;
               CHECK_PZ3;
               break;

            case 40:
            case 50:
            case 91:
            case 99:  /* Variante 3 */
#if DEBUG>0
               if(retvals){
                  retvals->methode="57c";
                  retvals->pz_methode=3057;
               }
#endif
               return OK_NO_CHK;

            default:   /* Variante 4 */
#if DEBUG>0
               if(retvals){
                  retvals->methode="57d";
                  retvals->pz_methode=4057;
               }
#endif
               if(tmp==0)return INVALID_KTO; /* Kontonummern müssen mit 01 bis 31 beginnen */
               tmp=(kto[2]-'0')*10+kto[3]-'0';
               if(tmp==0 || tmp>12)return INVALID_KTO;
               tmp=(kto[6]-'0')*100+(kto[7]-'0')*10+kto[8]-'0';
               if(tmp>=500)return INVALID_KTO;
               return OK_NO_CHK; /* kein Berechnungsverfahren angegeben, benutze auch 9 wie in Variante 3 (???) */
         }


/*  Berechnung nach der Methode 58 +§§§4 */
/*
 * ######################################################################
 * #   Berechnung nach der Methode 58 (geändert zum 4.3.2002)           #
 * ######################################################################
 * # Die Kontonummer (mindestens 6-stellig) ist durch linksbündige      #
 * # Nullenauffüllung 10-stellig darzustellen. Danach ist die 10.       #
 * # Stelle die Prüfziffer. Die Stellen 5 bis 9 werden von rechts nach  #
 * # links mit den Ziffern 2, 3, 4, 5, 6 multipliziert. Die restliche   #
 * # Berechnung und die Ergebnisse entsprechen dem Verfahren 02.        #
 * #                                                                    #
 * # Ergibt die Division einen Rest von 0, so ist die Prüfziffer = 0.   #
 * # Bei einem Rest von 1 ist die Kontonummer falsch.                   #
 * ######################################################################
 */

      case 58:
#if DEBUG>0
         if(retvals){
            retvals->methode="58";
            retvals->pz_methode=58;
         }
#endif
         if(kto[0]=='0' && kto[1]=='0' && kto[2]=='0' && kto[3]=='0' && kto[4]=='0')
            return INVALID_KTO;
         pz = (kto[4]-'0') * 6
            + (kto[5]-'0') * 5
            + (kto[6]-'0') * 4
            + (kto[7]-'0') * 3
            + (kto[8]-'0') * 2;

         MOD_11_176;   /* pz%=11 */
         if(pz)pz=11-pz;
         INVALID_PZ10;
         CHECK_PZ10;

/*  Berechnung nach der Methode 59 +§§§4 */
/*
 * ######################################################################
 * #    Berechnung nach der Methode 59 (geändert seit 3.12.2001)        #
 * ######################################################################
 * # Modulus 10, Gewichtung 2, 1, 2, 1, 2, 1, 2, 1, 2.                  #
 * # Die Berechnung erfolgt wie bei Verfahren 00; es ist jedoch         #
 * # zu beachten, daß Kontonummern, die kleiner als 9-stellig sind,     #
 * # nicht in die Prüfziffernberechnung einbezogen werden.              #
 * ######################################################################
 */
      case 59:
#if DEBUG>0
         if(retvals){
            retvals->methode="59";
            retvals->pz_methode=59;
         }
#endif
         if(*kto=='0' && *(kto+1)=='0')return OK_NO_CHK;
#ifdef __ALPHA
         pz = ((kto[0]<'5') ? (kto[0]-'0')*2 : (kto[0]-'0')*2-9)
            +  (kto[1]-'0')
            + ((kto[2]<'5') ? (kto[2]-'0')*2 : (kto[2]-'0')*2-9)
            +  (kto[3]-'0')
            + ((kto[4]<'5') ? (kto[4]-'0')*2 : (kto[4]-'0')*2-9)
            +  (kto[5]-'0')
            + ((kto[6]<'5') ? (kto[6]-'0')*2 : (kto[6]-'0')*2-9)
            +  (kto[7]-'0')
            + ((kto[8]<'5') ? (kto[8]-'0')*2 : (kto[8]-'0')*2-9);
#else

         pz=(kto[1]-'0')+(kto[3]-'0')+(kto[5]-'0')+(kto[7]-'0');
         if(kto[0]<'5')pz+=(kto[0]-'0')*2; else pz+=(kto[0]-'0')*2-9;
         if(kto[2]<'5')pz+=(kto[2]-'0')*2; else pz+=(kto[2]-'0')*2-9;
         if(kto[4]<'5')pz+=(kto[4]-'0')*2; else pz+=(kto[4]-'0')*2-9;
         if(kto[6]<'5')pz+=(kto[6]-'0')*2; else pz+=(kto[6]-'0')*2-9;
         if(kto[8]<'5')pz+=(kto[8]-'0')*2; else pz+=(kto[8]-'0')*2-9;
#endif
         MOD_10_80;   /* pz%=10 */
         if(pz)pz=10-pz;
         CHECK_PZ10;

/* Berechnungsmethoden 60 bis 69 +§§§3
   Berechnung nach der Methode 60 +§§§4 */
/*
 * ######################################################################
 * #              Berechnung nach der Methode 60                        #
 * ######################################################################
 * # Modulus 10, Gewichtung 2, 1, 2, 1, 2, 1, 2.                        #
 * # Die Berechnung erfolgt wie bei Verfahren 00. Es ist jedoch zu      #
 * # beachten, daß die zweistellige Unterkontonummer (Stellen 1 und     #
 * # 2) nicht in das Prüfziffernverfahren mit einbezogen werden darf.   #
 * # Die für die Berechnung relevante siebenstellige Grundnummer        #
 * # befindet sich in den Stellen 3 bis 9, die Prüfziffer in der        #
 * # Stelle 10.                                                         #
 * ######################################################################
 */
      case 60:
#if DEBUG>0
         if(retvals){
            retvals->methode="60";
            retvals->pz_methode=60;
         }
#endif
#ifdef __ALPHA
         pz = ((kto[2]<'5') ? (kto[2]-'0')*2 : (kto[2]-'0')*2-9)
            +  (kto[3]-'0')
            + ((kto[4]<'5') ? (kto[4]-'0')*2 : (kto[4]-'0')*2-9)
            +  (kto[5]-'0')
            + ((kto[6]<'5') ? (kto[6]-'0')*2 : (kto[6]-'0')*2-9)
            +  (kto[7]-'0')
            + ((kto[8]<'5') ? (kto[8]-'0')*2 : (kto[8]-'0')*2-9);
#else
         pz=(kto[3]-'0')+(kto[5]-'0')+(kto[7]-'0');
         if(kto[2]<'5')pz+=(kto[2]-'0')*2; else pz+=(kto[2]-'0')*2-9;
         if(kto[4]<'5')pz+=(kto[4]-'0')*2; else pz+=(kto[4]-'0')*2-9;
         if(kto[6]<'5')pz+=(kto[6]-'0')*2; else pz+=(kto[6]-'0')*2-9;
         if(kto[8]<'5')pz+=(kto[8]-'0')*2; else pz+=(kto[8]-'0')*2-9;
#endif
         MOD_10_80;   /* pz%=10 */
         if(pz)pz=10-pz;
         CHECK_PZ10;

/*  Berechnung nach der Methode 61 +§§§4 */
/*
 * ######################################################################
 * #              Berechnung nach der Methode 61                        #
 * ######################################################################
 * # Modulus 10, Gewichtung 2, 1, 2, 1, 2, 1, 2.                        #
 * # Darstellung der Kontonummer: B B B S S S S P A U (10-stellig).     #
 * # B = Betriebsstellennummer                                          #
 * # S = Stammnummer                                                    #
 * # P = Prüfziffer                                                     #
 * # A = Artziffer                                                      #
 * # U = Unternummer                                                    #
 * # Die Berechnung erfolgt wie bei Verfahren 00 über Betriebs-         #
 * # stellennummer und Stammnummer mit der Gewichtung 2, 1, 2, 1,       #
 * # 2, 1, 2.                                                           #
 * # Ist die Artziffer (neunte Stelle der Kontonummer) eine 8, so       #
 * # werden die neunte und zehnte Stelle der Kontonummer in die         #
 * # Prüfziffernermittlung einbezogen. Die Berechnung erfolgt dann      #
 * # über Betriebsstellennummer, Stammnummer, Artziffer und Unter-      #
 * # nummer mit der Gewichtung 2, 1, 2, 1, 2, 1, 2, 1, 2.               #
 * ######################################################################
 */
      case 61:
         if(*(kto+8)=='8'){
#if DEBUG>0
      case 2061:
         if(retvals){
            retvals->methode="61b";
            retvals->pz_methode=2061;
         }
#endif
#ifdef __ALPHA
            pz = ((kto[0]<'5') ? (kto[0]-'0')*2 : (kto[0]-'0')*2-9)
               +  (kto[1]-'0')
               + ((kto[2]<'5') ? (kto[2]-'0')*2 : (kto[2]-'0')*2-9)
               +  (kto[3]-'0')
               + ((kto[4]<'5') ? (kto[4]-'0')*2 : (kto[4]-'0')*2-9)
               +  (kto[5]-'0')
               + ((kto[6]<'5') ? (kto[6]-'0')*2 : (kto[6]-'0')*2-9)
               +  (kto[8]-'0')
               + ((kto[9]<'5') ? (kto[9]-'0')*2 : (kto[9]-'0')*2-9);
#else

            pz=(kto[1]-'0')+(kto[3]-'0')+(kto[5]-'0')+(kto[8]-'0');
            if(kto[0]<'5')pz+=(kto[0]-'0')*2; else pz+=(kto[0]-'0')*2-9;
            if(kto[2]<'5')pz+=(kto[2]-'0')*2; else pz+=(kto[2]-'0')*2-9;
            if(kto[4]<'5')pz+=(kto[4]-'0')*2; else pz+=(kto[4]-'0')*2-9;
            if(kto[6]<'5')pz+=(kto[6]-'0')*2; else pz+=(kto[6]-'0')*2-9;
            if(kto[9]<'5')pz+=(kto[9]-'0')*2; else pz+=(kto[9]-'0')*2-9;
#endif
            MOD_10_80;   /* pz%=10 */
            if(pz)pz=10-pz;
            CHECK_PZ8;
         }
         else{
#if DEBUG>0
      case 1061:
         if(retvals){
            retvals->methode="61a";
            retvals->pz_methode=1061;
         }
#endif
#ifdef __ALPHA
            pz = ((kto[0]<'5') ? (kto[0]-'0')*2 : (kto[0]-'0')*2-9)
               +  (kto[1]-'0')
               + ((kto[2]<'5') ? (kto[2]-'0')*2 : (kto[2]-'0')*2-9)
               +  (kto[3]-'0')
               + ((kto[4]<'5') ? (kto[4]-'0')*2 : (kto[4]-'0')*2-9)
               +  (kto[5]-'0')
               + ((kto[6]<'5') ? (kto[6]-'0')*2 : (kto[6]-'0')*2-9);
#else
            pz=(kto[1]-'0')+(kto[3]-'0')+(kto[5]-'0');
            if(kto[0]<'5')pz+=(kto[0]-'0')*2; else pz+=(kto[0]-'0')*2-9;
            if(kto[2]<'5')pz+=(kto[2]-'0')*2; else pz+=(kto[2]-'0')*2-9;
            if(kto[4]<'5')pz+=(kto[4]-'0')*2; else pz+=(kto[4]-'0')*2-9;
            if(kto[6]<'5')pz+=(kto[6]-'0')*2; else pz+=(kto[6]-'0')*2-9;
#endif
            MOD_10_80;   /* pz%=10 */
            if(pz)pz=10-pz;
            CHECK_PZ8;
         }

/*  Berechnung nach der Methode 62 +§§§4 */
/*
 * ######################################################################
 * #              Berechnung nach der Methode 62                        #
 * ######################################################################
 * # Modulus 10, Gewichtung 2, 1, 2, 1, 2.                              #
 * # Die beiden ersten und die beiden letzten Stellen sind              #
 * # nicht zu berücksichtigen. Die Stellen drei bis sieben              #
 * # sind von rechts nach links mit den Ziffern 2, 1, 2, 1, 2           #
 * # zu multiplizieren. Aus zweistelligen Einzelergebnissen             #
 * # ist eine Quersumme zu bilden. Alle Ergebnisse sind dann            #
 * # zu addieren. Die Differenz zum nächsten Zehner ergibt die          #
 * # Prüfziffer auf Stelle acht. Ist die Differenz 10, ist die          #
 * # Prüfziffer 0.                                                      #
 * ######################################################################
 */
      case 62:
#if DEBUG>0
         if(retvals){
            retvals->methode="62";
            retvals->pz_methode=62;
         }
#endif
#ifdef __ALPHA
         pz = ((kto[2]<'5') ? (kto[2]-'0')*2 : (kto[2]-'0')*2-9)
            +  (kto[3]-'0')
            + ((kto[4]<'5') ? (kto[4]-'0')*2 : (kto[4]-'0')*2-9)
            +  (kto[5]-'0')
            + ((kto[6]<'5') ? (kto[6]-'0')*2 : (kto[6]-'0')*2-9);
#else

         pz=(kto[3]-'0')+(kto[5]-'0');
         if(kto[2]<'5')pz+=(kto[2]-'0')*2; else pz+=(kto[2]-'0')*2-9;
         if(kto[4]<'5')pz+=(kto[4]-'0')*2; else pz+=(kto[4]-'0')*2-9;
         if(kto[6]<'5')pz+=(kto[6]-'0')*2; else pz+=(kto[6]-'0')*2-9;
#endif
         MOD_10_40;   /* pz%=10 */
         if(pz)pz=10-pz;
         CHECK_PZ8;

/*  Berechnung nach der Methode 63 +§§§4 */
/*
 * ######################################################################
 * #              Berechnung nach der Methode 63                        #
 * ######################################################################
 * # Modulus 10, Gewichtung 2, 1, 2, 1, 2, 1.                           #
 * # Die für die Berechnung relevante 6-stellige Grundnummer            #
 * # (Kundennummer) befindet sich in den Stellen 2-7, die Prüfziffer    #
 * # in Stelle 8 der Kontonummer. Die zweistellige Unterkontonummer     #
 * # (Stellen 9-10) ist nicht in das Prüfziffernverfahren einzu-        #
 * # beziehen. Die einzelnen Stellen der Grundnummer sind von rechts    #
 * # nach links mit den Ziffern 2, 1, 2, 1, 2, 1 zu multiplizieren.     #
 * # Die jeweiligen Produkte werden addiert, nachdem jeweils aus        #
 * # den zweistelligen Produkten die Quersumme gebildet wurde           #
 * # (z.B. Produkt 16 = Quersumme 7). Nach der Addition bleiben         #
 * # außer der Einerstelle alle anderen Stellen unberücksichtigt.       #
 * # Die Einerstelle wird von dem Wert 10 subtrahiert. Das Ergebnis     #
 * # ist die Prüfziffer (Stelle 8). Hat die Einerstelle den Wert 0,     #
 * # ist die Prüfziffer 0. Ausnahmen:                                   #
 * # Ist die Ziffer in Stelle 1 vor der sechsstelligen Grundnummer      #
 * # nicht 0, ist das Ergebnis als falsch zu werten.                    #
 * # Ist die Unterkontonummer 00, kann es vorkommen, daß sie auf        #
 * # den Zahlungsverkehrsbelegen nicht angegeben ist, die Kontonummer   #
 * # jedoch um führende Nullen ergänzt wurde. In diesem Fall sind       #
 * # z.B. die Stellen 1-3 000, die Prüfziffer ist an der Stelle 10.     #
 * ######################################################################
 */
      case 63:
      if(*kto!='0')return INVALID_KTO;

      /* der Test auf evl. weggelassenes Unterkonto erfolgt nun am Anfang
       * der Methode, nicht mehr am Ende; dadurch werden einige bislang (wohl
       * irrtümlich) als korrekt angesehene Konten als falsch ausgegeben.
       * Die Beschreibung ist nicht eindeutig; scheinbar werden allerdings
       * öfters Unterkonten weggelassen, als daß führende Nullen (an dieser
       * Stelle, nicht allgemein bei den Banken ;-) ) auftreten (vielen Dank
       * an Th. Franz für den Hinweis).
       */
      if(*(kto+1)=='0' && *(kto+2)=='0'){ /* evl. Unterkonto weggelassen */
#if DEBUG>0
      case 2063:
         if(retvals){
            retvals->methode="63b";
            retvals->pz_methode=2063;
         }
#endif
#ifdef __ALPHA
         pz =   (kto[3]-'0')
            +  ((kto[4]<'5') ? (kto[4]-'0')*2 : (kto[4]-'0')*2-9)
            +   (kto[5]-'0')
            +  ((kto[6]<'5') ? (kto[6]-'0')*2 : (kto[6]-'0')*2-9)
            +   (kto[7]-'0')
            +  ((kto[8]<'5') ? (kto[8]-'0')*2 : (kto[8]-'0')*2-9);
#else
         pz=(kto[3]-'0')+(kto[5]-'0')+(kto[7]-'0');
         if(kto[4]<'5')pz+=(kto[4]-'0')*2; else pz+=(kto[4]-'0')*2-9;
         if(kto[6]<'5')pz+=(kto[6]-'0')*2; else pz+=(kto[6]-'0')*2-9;
         if(kto[8]<'5')pz+=(kto[8]-'0')*2; else pz+=(kto[8]-'0')*2-9;
#endif
         MOD_10_80;   /* pz%=10 */
         if(pz)pz=10-pz;
         CHECK_PZ10;
      }
#if DEBUG>0
      case 1063:
         if(retvals){
            retvals->methode="63a";
            retvals->pz_methode=1063;
         }
#endif
#ifdef __ALPHA
         pz =   (kto[1]-'0')
            +  ((kto[2]<'5') ? (kto[2]-'0')*2 : (kto[2]-'0')*2-9)
            +   (kto[3]-'0')
            +  ((kto[4]<'5') ? (kto[4]-'0')*2 : (kto[4]-'0')*2-9)
            +   (kto[5]-'0')
            +  ((kto[6]<'5') ? (kto[6]-'0')*2 : (kto[6]-'0')*2-9);
#else
         pz=(kto[1]-'0')+(kto[3]-'0')+(kto[5]-'0');
         if(kto[2]<'5')pz+=(kto[2]-'0')*2; else pz+=(kto[2]-'0')*2-9;
         if(kto[4]<'5')pz+=(kto[4]-'0')*2; else pz+=(kto[4]-'0')*2-9;
         if(kto[6]<'5')pz+=(kto[6]-'0')*2; else pz+=(kto[6]-'0')*2-9;
#endif
         MOD_10_80;   /* pz%=10 */
         if(pz)pz=10-pz;
         CHECK_PZ8;

/*  Berechnung nach der Methode 64 +§§§4 */
/*
 * ######################################################################
 * #              Berechnung nach der Methode 64                        #
 * ######################################################################
 * # Modulus 11, Gewichtung 9, 10, 5, 8, 4, 2.                          #
 * #  Die Kontonummer ist 10-stellig. Die für die Berechnung relevanten #
 * #  Stellen der Kontonummer befinden sich in den Stellen 1 bis 6 und  #
 * #  werden von links nach rechts mit den Ziffern 9, 10, 5, 8, 4, 2    #
 * #  multipliziert. Die weitere Berechnung und Ergebnisse entsprechen  #
 * #  dem Verfahren 06. Die Prüfziffer befindet sich in Stelle 7 der    #
 * #  Kontonummer.                                                      #
 * ######################################################################
 */
      case 64:
#if DEBUG>0
         if(retvals){
            retvals->methode="64";
            retvals->pz_methode=64;
         }
#endif
         pz = (kto[0]-'0') * 9
            + (kto[1]-'0') * 10
            + (kto[2]-'0') * 5
            + (kto[3]-'0') * 8
            + (kto[4]-'0') * 4
            + (kto[5]-'0') * 2;

         MOD_11_176;   /* pz%=11 */
         if(pz<=1)
            pz=0;
         else
            pz=11-pz;
         CHECK_PZ7;

/*  Berechnung nach der Methode 65 +§§§4 */
/*
 * ######################################################################
 * #              Berechnung nach der Methode 65                        #
 * ######################################################################
 * # Modulus 10, Gewichtung 2, 1, 2, 1, 2, 1, 2.                        #
 * # Die Kontonummer ist zehnstellig: G G G S S S S P K U               #
 * # G = Geschäftstellennummer                                          #
 * # S = Stammnummer                                                    #
 * # P = Prüfziffer                                                     #
 * # K = Kontenartziffer                                                #
 * # U = Unterkontonummer                                               #
 * # Die Berechnung erfolgt wie bei Verfahren 00 über Geschäfts-        #
 * # stellennummer und Stammnummer mit der Gewichtung 2, 1, 2,...       #
 * # Ausnahme: Ist die Kontenartziffer (neunte Stelle der Konto-        #
 * # nummer) eine 9, so werden die neunte und zehnte Stelle der         #
 * # Kontonummer in die Prüfziffernermittlung einbezogen. Die           #
 * # Berechnung erfolgt dann über die Geschäftsstellennummer,           #
 * # Stammnummer, Kontenartziffer und Unterkontonummer mit der          #
 * # Gewichtung 2, 1, 2, 1, 2, 1, 2, 1, 2.                              #
 * ######################################################################
 */
      case 65:
#if DEBUG>0
         if(retvals){
            retvals->methode="65";
            retvals->pz_methode=65;
         }
#endif
#ifdef __ALPHA
         pz = ((kto[0]<'5') ? (kto[0]-'0')*2 : (kto[0]-'0')*2-9)
            +  (kto[1]-'0')
            + ((kto[2]<'5') ? (kto[2]-'0')*2 : (kto[2]-'0')*2-9)
            +  (kto[3]-'0')
            + ((kto[4]<'5') ? (kto[4]-'0')*2 : (kto[4]-'0')*2-9)
            +  (kto[5]-'0')
            + ((kto[6]<'5') ? (kto[6]-'0')*2 : (kto[6]-'0')*2-9);
#else

         pz=(kto[1]-'0')+(kto[3]-'0')+(kto[5]-'0');
         if(kto[0]<'5')pz+=(kto[0]-'0')*2; else pz+=(kto[0]-'0')*2-9;
         if(kto[2]<'5')pz+=(kto[2]-'0')*2; else pz+=(kto[2]-'0')*2-9;
         if(kto[4]<'5')pz+=(kto[4]-'0')*2; else pz+=(kto[4]-'0')*2-9;
         if(kto[6]<'5')pz+=(kto[6]-'0')*2; else pz+=(kto[6]-'0')*2-9;
#endif
         MOD_10_80;   /* pz%=10 */
         if(kto[8]=='9'){
            p1=(kto[9]-'0')*2;
            if(p1>9)p1-=9;
            pz+=p1+9;
         }
         pz=pz%10;
         if(pz)pz=10-pz;
         CHECK_PZ8;

/*  Berechnung nach der Methode 66 +§§§4 */
/*
 * ######################################################################
 * #              Berechnung nach der Methode 66                        #
 * ######################################################################
 * # Aufbau der 9-stelligen Kontonummer (innerhalb des                  #
 * # zwischenbetrieblich 10-stelligen Feldes)                           #
 * # Stelle    1    = gehört nicht zur Kontonummer, muss                #
 * #                  daher 0 sein                                      #
 * #           2    = Stammnunmmer                                      #
 * #           3-4  = Unterkontonummer, wird bei der Prüfziffer-        #
 * #                  berechnung nicht berücksichtigt                   #
 * #           5-9  = Stammnummer                                       #
 * #           10   = Prüfziffer                                        #
 * # Der 9-stelligen Kontonummer wird für die Prüfzifferberechnung      #
 * # eine 0 vorangestellt. Die Prüfziffer steht in Stelle 10. Die für   #
 * # die Berechnung relevante 6-stellige Stammnummer (Kundenummer)      #
 * # befindet sich in den Stellen 2 und  5 bis 9. Die zweistellige      #
 * # Unterkontonummer (Stellen 3 und 4) wird nicht in das               #
 * # Prüfzifferberechnungsverfahren mit einbezogen und daher mit 0      #
 * # gewichtet. Die einzelnen Stellen der Stammnummer sind von rechts   #
 * # nach links mit den Ziffern 2, 3, 4, 5, 6, 0, 0, 7 zu               #
 * # multiplizieren. Die jeweiligen Produkte werden addiert. Die        #
 * # Summe ist durch 11 zu dividieren. Bei einem verbleibenden Rest     #
 * # von 0 ist die Prüfziffer 1. Bei einem Rest von 1 ist die           #
 * # Prüfziffer 0 Verbleibt ein Rest von 2 bis 10, so wird dieser vom   #
 * # Divison (11) subtrahiert. Die Differenz ist dann die Prüfziffer.   #
 * ######################################################################
 */
      case 66:
#if DEBUG>0
         if(retvals){
            retvals->methode="66";
            retvals->pz_methode=66;
         }
#endif
         if(*kto!='0')return INVALID_KTO;
         pz = (kto[1]-'0') * 7
            + (kto[4]-'0') * 6
            + (kto[5]-'0') * 5
            + (kto[6]-'0') * 4
            + (kto[7]-'0') * 3
            + (kto[8]-'0') * 2;

         MOD_11_176;   /* pz%=11 */
         if(pz<2)
            pz=1-pz;
         else
            pz=11-pz;
         CHECK_PZ10;

/*  Berechnung nach der Methode 67 +§§§4 */
/*
 * ######################################################################
 * #              Berechnung nach der Methode 67                        #
 * ######################################################################
 * # Modulus 10, Gewichtung 2, 1, 2, 1, 2, 1, 2.                        #
 * # Die Kontonummer ist zehnstellig. Die Berechnung erfolgt wie bei    #
 * # Verfahren 00. Es ist jedoch zu beachten, daß die zweistellige      #
 * # Unterkontonummer (Stellen 9 und 10) nicht in das Prüfziffern-      #
 * # verfahren mit einbezogen werden darf. Die für die Berechnung       #
 * # relevante siebenstellige Stammnummer befindet sich in den          #
 * # Stellen 1 bis 7, die Prüfziffer in der Stelle 8.                   #
 * ######################################################################
 */
      case 67:
#if DEBUG>0
         if(retvals){
            retvals->methode="67";
            retvals->pz_methode=67;
         }
#endif
#ifdef __ALPHA
         pz = ((kto[0]<'5') ? (kto[0]-'0')*2 : (kto[0]-'0')*2-9)
            +  (kto[1]-'0')
            + ((kto[2]<'5') ? (kto[2]-'0')*2 : (kto[2]-'0')*2-9)
            +  (kto[3]-'0')
            + ((kto[4]<'5') ? (kto[4]-'0')*2 : (kto[4]-'0')*2-9)
            +  (kto[5]-'0')
            + ((kto[6]<'5') ? (kto[6]-'0')*2 : (kto[6]-'0')*2-9);
#else

         pz=(kto[1]-'0')+(kto[3]-'0')+(kto[5]-'0');
         if(kto[0]<'5')pz+=(kto[0]-'0')*2; else pz+=(kto[0]-'0')*2-9;
         if(kto[2]<'5')pz+=(kto[2]-'0')*2; else pz+=(kto[2]-'0')*2-9;
         if(kto[4]<'5')pz+=(kto[4]-'0')*2; else pz+=(kto[4]-'0')*2-9;
         if(kto[6]<'5')pz+=(kto[6]-'0')*2; else pz+=(kto[6]-'0')*2-9;
#endif
         MOD_10_80;   /* pz%=10 */
         if(pz)pz=10-pz;
         CHECK_PZ8;

/*  Berechnung nach der Methode 68 +§§§4 */
/*
 * ######################################################################
 * #              Berechnung nach der Methode 68                        #
 * ######################################################################
 * # Modulus 10, Gewichtung 2, 1, 2, 1, 2, 1, 2, 1, 2.                  #
 * # Die Kontonummern sind 6 bis 10stellig und enthalten keine          #
 * # führenden Nullen. Die erste Stelle von rechts ist die              #
 * # Prüfziffer. Die Berechnung erfolgt wie bei Verfahren 00,           #
 * # hierbei sind jedoch folgende Besonderheiten zu beachten:           #
 * # Bei 10stelligen Kontonummern erfolgt die Berechnung für die        #
 * # 2. bis 7. Stelle (von rechts!). Stelle 7 muß eine 9 sein.          #
 * # 6 bis 9stellige Kontonummern sind in zwei Varianten prüfbar.       #
 * # Variante 1: voll prüfbar.                                          #
 * # Ergibt die Berechnung nach Variante 1 einen Prüfziffernfehler,     #
 * # muß Variante 2 zu einer korrekten Prüfziffer führen.               #
 * # Variante 2: Stellen 7 und 8 werden nicht geprüft.                  #
 * # 9stellige Kontonummern im Nummerenbereich 400000000 bis            #
 * # 4999999999 sind nicht prüfbar, da diese Nummern keine              #
 * # Prüfziffer enthalten.                                              #
 * ######################################################################
 */
      case 68:
#if DEBUG>0
         if(retvals){
            retvals->methode="68";
            retvals->pz_methode=68;
         }
#endif
            /* die Kontonummer muß mindestens 6-stellig sein (ohne führende Nullen) */
         if(kto[0]=='0' && kto[1]=='0' && kto[2]=='0' && kto[3]=='0' && kto[4]=='0')
            return INVALID_KTO;

            /* Sonderfall: keine Prüfziffer */
         if(*kto=='0' && *(kto+1)=='4'){
#if DEBUG>0
            pz= *(kto+9)-'0';
#endif
            return OK_NO_CHK;
         }

            /* 10stellige Kontonummern */
#if DEBUG>0
      case 1068:
         if(retvals){
            retvals->methode="68a";
            retvals->pz_methode=1068;
         }
#endif
         if(*kto!='0'){
            if(*(kto+3)!='9')return INVALID_KTO;
#ifdef __ALPHA
         pz = 9           /* 7. Stelle von rechts */
            + ((kto[4]<'5') ? (kto[4]-'0')*2 : (kto[4]-'0')*2-9)
            +  (kto[5]-'0')
            + ((kto[6]<'5') ? (kto[6]-'0')*2 : (kto[6]-'0')*2-9)
            +  (kto[7]-'0')
            + ((kto[8]<'5') ? (kto[8]-'0')*2 : (kto[8]-'0')*2-9);
#else
         pz=9+(kto[5]-'0')+(kto[7]-'0');
         if(kto[4]<'5')pz+=(kto[4]-'0')*2; else pz+=(kto[4]-'0')*2-9;
         if(kto[6]<'5')pz+=(kto[6]-'0')*2; else pz+=(kto[6]-'0')*2-9;
         if(kto[8]<'5')pz+=(kto[8]-'0')*2; else pz+=(kto[8]-'0')*2-9;
#endif
         MOD_10_40;   /* pz%=10 */
            if(pz)pz=10-pz;
            CHECK_PZ10;
         }

            /* 6 bis 9stellige Kontonummern: Variante 1 */
#if DEBUG>0
      case 2068:
         if(retvals){
            retvals->methode="68b";
            retvals->pz_methode=2068;
         }
#endif
#ifdef __ALPHA
         pz =  (kto[1]-'0')
            + ((kto[2]<'5') ? (kto[2]-'0')*2 : (kto[2]-'0')*2-9)
            +  (kto[3]-'0')
            + ((kto[4]<'5') ? (kto[4]-'0')*2 : (kto[4]-'0')*2-9)
            +  (kto[5]-'0')
            + ((kto[6]<'5') ? (kto[6]-'0')*2 : (kto[6]-'0')*2-9)
            +  (kto[7]-'0')
            + ((kto[8]<'5') ? (kto[8]-'0')*2 : (kto[8]-'0')*2-9);
#else
         pz=(kto[1]-'0')+(kto[3]-'0')+(kto[5]-'0')+(kto[7]-'0');
         if(kto[2]<'5')pz+=(kto[2]-'0')*2; else pz+=(kto[2]-'0')*2-9;
         if(kto[4]<'5')pz+=(kto[4]-'0')*2; else pz+=(kto[4]-'0')*2-9;
         if(kto[6]<'5')pz+=(kto[6]-'0')*2; else pz+=(kto[6]-'0')*2-9;
         if(kto[8]<'5')pz+=(kto[8]-'0')*2; else pz+=(kto[8]-'0')*2-9;
#endif
         MOD_10_80;   /* pz%=10 */
         if(pz)pz=10-pz;
         CHECK_PZX10;

            /* 6 bis 9stellige Kontonummern: Variante 2 */
#if DEBUG>0
      case 3068:
         if(retvals){
            retvals->methode="68c";
            retvals->pz_methode=3068;
         }
#endif
#ifdef __ALPHA
         pz =  (kto[1]-'0')
            + ((kto[4]<'5') ? (kto[4]-'0')*2 : (kto[4]-'0')*2-9)
            +  (kto[5]-'0')
            + ((kto[6]<'5') ? (kto[6]-'0')*2 : (kto[6]-'0')*2-9)
            +  (kto[7]-'0')
            + ((kto[8]<'5') ? (kto[8]-'0')*2 : (kto[8]-'0')*2-9);
#else

         pz=(kto[1]-'0')+(kto[5]-'0')+(kto[7]-'0');
         if(kto[4]<'5')pz+=(kto[4]-'0')*2; else pz+=(kto[4]-'0')*2-9;
         if(kto[6]<'5')pz+=(kto[6]-'0')*2; else pz+=(kto[6]-'0')*2-9;
         if(kto[8]<'5')pz+=(kto[8]-'0')*2; else pz+=(kto[8]-'0')*2-9;
#endif
         MOD_10_40;   /* pz%=10 */
         if(pz)pz=10-pz;
         CHECK_PZ10;

/*  Berechnung nach der Methode 69 +§§§4 */
/*
 * ######################################################################
 * #              Berechnung nach der Methode 69                        #
 * ######################################################################
 * # Modulus 11, Gewichtung 2, 3, 4, 5, 6, 7, 8.                        #
 * # Die Berechnung erfolgt wie bei Verfahren 28. Ergibt die            #
 * # Berechnung einen Prüfziffernfehler, so ist die Prüfziffer          #
 * # nach Variante II zu ermitteln (s.u.).                              #
 * # Ausnahmen:                                                         #
 * # Für den Kontonummernkreis 9300000000 - 9399999999 ist keine        #
 * # Prüfziffernberechnung möglich = Kennziffer 09.                     #
 * # Für den Kontonummernkreis 9700000000 - 9799999999 ist die          #
 * # Prüfziffernberechnung wie folgt vorzunehmen (Variante II):         #
 * # Die Position der einzelnen Ziffern von rechts nach links           #
 * # innerhalb der Kontonummer gibt die Zeile 1 bis 4 der Trans-        #
 * # formationstabelle an. Aus ihr sind die Übersetzungswerte zu        #
 * # summieren. Die Einerstelle wird von 10 subtrahiert und stellt      #
 * # die Prüfziffer dar.                                                #
 * # Transformationstabelle:                                            #
 * # Ziffer    : 0123456789                                             #
 * # Zeile 1   : 0159374826                                             #
 * # Zeile 2   : 0176983254                                             #
 * # Zeile 3   : 0184629573                                             #
 * # Zeile 4   : 0123456789                                             #
 * ######################################################################
 */
      case 69:
#if DEBUG>0
         if(retvals){
            retvals->methode="69";
            retvals->pz_methode=69;
         }
#endif
            /* Sonderfall 93xxxxxxxx: Keine Prüfziffer */
         if(*kto=='9' && *(kto+1)=='3')return OK_NO_CHK;

            /* Variante 1 */
#if DEBUG>0
      case 1069:
         if(retvals){
            retvals->methode="69a";
            retvals->pz_methode=1069;
         }
#endif
            /* Sonderfall 97xxxxxxxx nur über Variante 2 */
         if(*kto!='9' || *(kto+1)!='7'){
            pz = (kto[0]-'0') * 8
               + (kto[1]-'0') * 7
               + (kto[2]-'0') * 6
               + (kto[3]-'0') * 5
               + (kto[4]-'0') * 4
               + (kto[5]-'0') * 3
               + (kto[6]-'0') * 2;

            MOD_11_176;   /* pz%=11 */
            if(pz<=1)
               pz=0;
            else
               pz=11-pz;
            CHECK_PZX8;
         }

            /* Variante 2 */
#if DEBUG>0
      case 2069:
         if(retvals){
            retvals->methode="69b";
            retvals->pz_methode=2069;
         }
#endif
         pz = m10h_digits[0][(unsigned int)(kto[0]-'0')]
            + m10h_digits[3][(unsigned int)(kto[1]-'0')]
            + m10h_digits[2][(unsigned int)(kto[2]-'0')]
            + m10h_digits[1][(unsigned int)(kto[3]-'0')]
            + m10h_digits[0][(unsigned int)(kto[4]-'0')]
            + m10h_digits[3][(unsigned int)(kto[5]-'0')]
            + m10h_digits[2][(unsigned int)(kto[6]-'0')]
            + m10h_digits[1][(unsigned int)(kto[7]-'0')]
            + m10h_digits[0][(unsigned int)(kto[8]-'0')];
         MOD_10_80;   /* pz%=10 */
         if(pz)pz=10-pz;
         CHECK_PZ10;

/* Berechnungsmethoden 70 bis 79 +§§§3
   Berechnung nach der Methode 70 +§§§4 */
/*
 * ######################################################################
 * #              Berechnung nach der Methode 70                        #
 * ######################################################################
 * # Modulus 11, Gewichtung 2, 3, 4, 5, 6, 7.                           #
 * # Die Kontonummer ist zehnstellig. Die einzelnen Stellen der         #
 * # Kontonummer sind von rechts nach links mit den Ziffern             #
 * # 2,3,4,5,6,7,2,3,4 zu multiplizieren. Die Berechnung erfolgt wie    #
 * # bei Verfahren 06.                                                  #
 * # Ausnahme: Ist die 4. Stelle der Kontonummer = 5 oder die 4. -      #
 * # 5. Stelle der Kontonummer = 69, so werden die Stellen 1 - 3        #
 * # nicht in die Prüfziffernermittlung einbezogen.                     #
 * ######################################################################
 */
      case 70:
#if DEBUG>0
         if(retvals){
            retvals->methode="70";
            retvals->pz_methode=70;
         }
#endif
         if(*(kto+3)=='5'){
            pz = 5 * 7
               + (kto[4]-'0') * 6
               + (kto[5]-'0') * 5
               + (kto[6]-'0') * 4
               + (kto[7]-'0') * 3
               + (kto[8]-'0') * 2;
            MOD_11_176;   /* pz%=11 */
         }
         else if(*(kto+3)=='6' && *(kto+4)=='9'){
            pz = 6 * 7
               + 9 * 6
               + (kto[5]-'0') * 5
               + (kto[6]-'0') * 4
               + (kto[7]-'0') * 3
               + (kto[8]-'0') * 2;
            MOD_11_176;   /* pz%=11 */
         }
         else{
            pz = (kto[0]-'0') * 4
               + (kto[1]-'0') * 3
               + (kto[2]-'0') * 2
               + (kto[3]-'0') * 7
               + (kto[4]-'0') * 6
               + (kto[5]-'0') * 5
               + (kto[6]-'0') * 4
               + (kto[7]-'0') * 3
               + (kto[8]-'0') * 2;
            MOD_11_176;   /* pz%=11 */
         }
         if(pz<=1)
            pz=0;
         else
            pz=11-pz;
         CHECK_PZ10;

/*  Berechnung nach der Methode 71 +§§§4 */
/*
 * ######################################################################
 * #              Berechnung nach der Methode 71                        #
 * ######################################################################
 * # Modulus 11, Gewichtung 6, 5, 4, 3, 2, 1.                           #
 * # Die Kontonummer ist immer 10-stellig. Die Stellen 2 bis 7          #
 * # sind von links nach rechts mit den Ziffern 6, 5, 4, 3, 2, 1 zu     #
 * # multiplizieren. Die Ergebnisse sind dann ohne Quersummenbildung    #
 * # zu addieren. Die Summe ist durch 11 zu dividieren.                 #
 * # Der verbleibende Rest wird vom Divisor (11) subtrahiert.           #
 * # Das Ergebnis ist die Prüfziffer.                                   #
 * # Ausnahmen: Verbleibt nach der Division durch 11 kein Rest, ist     #
 * # die Prüfziffer 0. Ergibt sich als Rest 1, entsteht bei der         #
 * # Subtraktion 11 ./. 1 = 10; die Zehnerstelle (1) ist dann           #
 * # die Prüfziffer.                                                    #
 * ######################################################################
 */
      case 71:
#if DEBUG>0
         if(retvals){
            retvals->methode="71";
            retvals->pz_methode=71;
         }
#endif
         pz = (kto[1]-'0') * 6
            + (kto[2]-'0') * 5
            + (kto[3]-'0') * 4
            + (kto[4]-'0') * 3
            + (kto[5]-'0') * 2
            + (kto[6]-'0');

         MOD_11_176;   /* pz%=11 */
         if(pz>1)pz=11-pz;
         CHECK_PZ10;

/*  Berechnung nach der Methode 72 +§§§4 */
/*
 * ######################################################################
 * #              Berechnung nach der Methode 72                        #
 * ######################################################################
 * # Modulus 10, Gewichtung 2, 1, 2, 1, 2, 1.                           #
 * # Die Kontonummer ist zehnstellig. Die Berechnung erfolgt wie bei    #
 * # Verfahren 00. Es ist jedoch zu beachten, daß die zweistellige      #
 * # Unterkontonummer (Stellen 1 und 2) und die Artziffer (Stelle 3)    #
 * # nicht in das Prüfziffernverfahren mit einbezogen werden.           #
 * # Die für die Berechnung relevante sechsstellige Kundennummer        #
 * # befindet sich in den Stellen 4 bis 9, die Prüfziffer in der        #
 * # Stelle 10.                                                         #
 * ######################################################################
 */
      case 72:
#if DEBUG>0
         if(retvals){
            retvals->methode="72";
            retvals->pz_methode=72;
         }
#endif
#ifdef __ALPHA
         pz =  (kto[3]-'0')
            + ((kto[4]<'5') ? (kto[4]-'0')*2 : (kto[4]-'0')*2-9)
            +  (kto[5]-'0')
            + ((kto[6]<'5') ? (kto[6]-'0')*2 : (kto[6]-'0')*2-9)
            +  (kto[7]-'0')
            + ((kto[8]<'5') ? (kto[8]-'0')*2 : (kto[8]-'0')*2-9);
#else
         pz=(kto[3]-'0')+(kto[5]-'0')+(kto[7]-'0');
         if(kto[4]<'5')pz+=(kto[4]-'0')*2; else pz+=(kto[4]-'0')*2-9;
         if(kto[6]<'5')pz+=(kto[6]-'0')*2; else pz+=(kto[6]-'0')*2-9;
         if(kto[8]<'5')pz+=(kto[8]-'0')*2; else pz+=(kto[8]-'0')*2-9;
#endif
         MOD_10_80;   /* pz%=10 */
         if(pz)pz=10-pz;
         CHECK_PZ10;

/*  Berechnung nach der Methode 73 +§§§4 */
/*
 * ######################################################################
 * #    Berechnung nach der Methode 73  (geändert zum 6.12.2004)        #
 * ######################################################################
 * #                                                                    #
 * # Die Kontonummer ist durch linksbündiges Auffüllen mit Nullen       #
 * # 10-stellig darzustellen. Die 10. Stelle der Kontonummer ist die    #
 * # Prüfziffer.                                                        #
 * #                                                                    #
 * # Variante 1:                                                        #
 * # Modulus 10, Gewichtung 2, 1, 2, 1, 2, 1                            #
 * # Die Stellen 4 bis 9 der Kontonummer werden von rechts nach links   #
 * # mit den Ziffern 2, 1, 2, 1, 2, 1 multipliziert. Die Berechnung und #
 * # Ergebnisse entsprechen dem Verfahren 00.                           #
 * #                                                                    #
 * # Führt die Berechnung nach Variante 1 zu einem Prüfzifferfehler,    #
 * # ist eine weitere Berechnung nach Variante 2 vorzunehmen.           #
 * #                                                                    #
 * # Variante 2:                                                        #
 * # Modulus 10, Gewichtung 2, 1, 2, 1, 2                               #
 * # Das Berechnungsverfahren entspricht Variante 1, es ist jedoch zu   #
 * # beachten, dass nur die Stellen 5 bis 9 in das Prüfziffern-         #
 * # berechnungsverfahren einbezogen werden.                            #
 * #                                                                    #
 * # Führt die Berechnung auch nach Variante 2 zu einem Prüfziffer-     #
 * # fehler, ist die Berechnung nach Variante 3 vorzunehmen:            #
 * #                                                                    #
 * # Variante 3                                                         #
 * # Modulus 7, Gewichtung 2, 1, 2, 1, 2 Das Berechnungsverfahren       #
 * # entspricht Variante 2. Die Summe der Produkt-Quersummen ist jedoch #
 * # durch 7 zu dividieren. Der verbleibende Rest wird vom Divisor (7)  #
 * # subtrahiert. Das Ergebnis ist die Prüfziffer. Verbleibt nach der   #
 * # Division kein Rest, ist die Prüfziffer = 0                         #
 * #                                                                    #
 * # Ausnahme:                                                          #
 * # Ist nach linksbündiger Auffüllung mit Nullen auf 10 Stellen die 3. #
 * # Stelle der Kontonummer = 9 (Sachkonten), so erfolgt die Berechnung #
 * # gemäß der Ausnahme in Methode 51 mit den gleichen Ergebnissen und  #
 * # Testkontonummern.                                                  #
 * ######################################################################
 */

      case 73:
                /* Ausnahme, Variante 1 */
         if(*(kto+2)=='9'){   /* Berechnung wie in Verfahren 51 */
#if DEBUG>0
      case 4073:
         if(retvals){
            retvals->methode="73d";
            retvals->pz_methode=4073;
         }
#endif
            pz =         9 * 8
            + (kto[3]-'0') * 7
            + (kto[4]-'0') * 6
            + (kto[5]-'0') * 5
            + (kto[6]-'0') * 4
            + (kto[7]-'0') * 3
            + (kto[8]-'0') * 2;

            MOD_11_176;   /* pz%=11 */
            if(pz<=1)
               pz=0;
            else
               pz=11-pz;
            CHECK_PZX10;

            /* Ausnahme, Variante 2 */
#if DEBUG>0
      case 5073:
         if(retvals){
            retvals->methode="73e";
            retvals->pz_methode=5073;
         }
#endif
               pz = (kto[0]-'0') * 10
               + (kto[1]-'0') * 9
               +            9 * 8
               + (kto[3]-'0') * 7
               + (kto[4]-'0') * 6
               + (kto[5]-'0') * 5
               + (kto[6]-'0') * 4
               + (kto[7]-'0') * 3
               + (kto[8]-'0') * 2;

               MOD_11_352;   /* pz%=11 */
               if(pz<=1)
                  pz=0;
               else
                  pz=11-pz;
               CHECK_PZ10;
            }

#if DEBUG>0
      case 1073:
         if(retvals){
            retvals->methode="73a";
            retvals->pz_methode=1073;
         }
#endif
#ifdef __ALPHA
         pz1= ((kto[4]<'5') ? (kto[4]-'0')*2 : (kto[4]-'0')*2-9)
            +  (kto[5]-'0')
            + ((kto[6]<'5') ? (kto[6]-'0')*2 : (kto[6]-'0')*2-9)
            +  (kto[7]-'0')
            + ((kto[8]<'5') ? (kto[8]-'0')*2 : (kto[8]-'0')*2-9);
#else

         pz1=(kto[5]-'0')+(kto[7]-'0');
         if(kto[4]<'5')pz1+=(kto[4]-'0')*2; else pz1+=(kto[4]-'0')*2-9;
         if(kto[6]<'5')pz1+=(kto[6]-'0')*2; else pz1+=(kto[6]-'0')*2-9;
         if(kto[8]<'5')pz1+=(kto[8]-'0')*2; else pz1+=(kto[8]-'0')*2-9;
#endif
         pz=pz1+(kto[3]-'0');
         MOD_10_80;   /* pz%=10 */
         if(pz)pz=10-pz;
         CHECK_PZX10;

#if DEBUG>0
      case 2073:
         if(retvals){
            retvals->methode="73b";
            retvals->pz_methode=2073;
         }
#endif
#if DEBUG>0
#ifdef __ALPHA
         pz = ((kto[4]<'5') ? (kto[4]-'0')*2 : (kto[4]-'0')*2-9)
            +  (kto[5]-'0')
            + ((kto[6]<'5') ? (kto[6]-'0')*2 : (kto[6]-'0')*2-9)
            +  (kto[7]-'0')
            + ((kto[8]<'5') ? (kto[8]-'0')*2 : (kto[8]-'0')*2-9);
#else

         pz=(kto[5]-'0')+(kto[7]-'0');
         if(kto[4]<'5')pz+=(kto[4]-'0')*2; else pz+=(kto[4]-'0')*2-9;
         if(kto[6]<'5')pz+=(kto[6]-'0')*2; else pz+=(kto[6]-'0')*2-9;
         if(kto[8]<'5')pz+=(kto[8]-'0')*2; else pz+=(kto[8]-'0')*2-9;
#endif
#else
         pz=pz1;
#endif
         MOD_10_80;   /* pz%=10 */
         if(pz)pz=10-pz;
         CHECK_PZX10;

#if DEBUG>0
      case 3073:
         if(retvals){
            retvals->methode="73c";
            retvals->pz_methode=3073;
         }
#endif
#if DEBUG
#ifdef __ALPHA
         pz = ((kto[4]<'5') ? (kto[4]-'0')*2 : (kto[4]-'0')*2-9)
            +  (kto[5]-'0')
            + ((kto[6]<'5') ? (kto[6]-'0')*2 : (kto[6]-'0')*2-9)
            +  (kto[7]-'0')
            + ((kto[8]<'5') ? (kto[8]-'0')*2 : (kto[8]-'0')*2-9);
#else

         pz=(kto[5]-'0')+(kto[7]-'0');
         if(kto[4]<'5')pz+=(kto[4]-'0')*2; else pz+=(kto[4]-'0')*2-9;
         if(kto[6]<'5')pz+=(kto[6]-'0')*2; else pz+=(kto[6]-'0')*2-9;
         if(kto[8]<'5')pz+=(kto[8]-'0')*2; else pz+=(kto[8]-'0')*2-9;
#endif
#else
         pz=pz1;
#endif
         MOD_7_56;   /* pz%=7 */
         if(pz)pz=7-pz;
         CHECK_PZ10;

/*  Berechnung nach der Methode 74 +§§§4 */
/*
 * ######################################################################
 * #    Berechnung nach der Methode 74 (geändert zum 4.6.2007)          #
 * ######################################################################
 * # Modulus 10, Gewichtung 2, 1, 2, 1, 2 ff.                           #
 * # Die Kontonummer (2- bis 10-stellig) ist durch linksbündige         #
 * # Nullenauffüllung 10-stellig darzustellen. Die 10. Stelle ist       #
 * # per Definition die Prüfziffer. Die für die Berechnung              #
 * # relevanten Stellen werden von rechts nach links mit den Ziffern    #
 * # 2, 1, 2, 1, 2 ff. multipliziert. Die weitere Berechnung und die    #
 * # Ergebnisse entsprechen dem Verfahren 00.                           #
 * #                                                                    #
 * # Ausnahme:                                                          #
 * # Bei 6-stelligen Kontonummern ist folgende Besonderheit zu          #
 * # beachten.                                                          #
 * # Ergibt die erste Berechnung der Prüfziffer nach dem Verfahren 00   #
 * # einen Prüfziffernfehler, so ist eine weitere Berechnung            #
 * # vorzunehmen. Hierbei ist die Summe der Produkte auf die nächste    #
 * # Halbdekade hochzurechnen. Die Differenz ist die Prüfziffer.        #
 * ######################################################################
 */
      case 74:
         if(kto[0]=='0' && kto[1]=='0' && kto[2]=='0' && kto[3]=='0' && kto[4]=='0'
               && kto[5]=='0' && kto[6]=='0' && kto[7]=='0' && kto[8]=='0')return INVALID_KTO;
#if DEBUG>0
      case 1074:
         if(retvals){
            retvals->methode="74a";
            retvals->pz_methode=1074;
         }
#endif
#ifdef __ALPHA
         pz = ((kto[0]<'5') ? (kto[0]-'0')*2 : (kto[0]-'0')*2-9)
            +  (kto[1]-'0')
            + ((kto[2]<'5') ? (kto[2]-'0')*2 : (kto[2]-'0')*2-9)
            +  (kto[3]-'0')
            + ((kto[4]<'5') ? (kto[4]-'0')*2 : (kto[4]-'0')*2-9)
            +  (kto[5]-'0')
            + ((kto[6]<'5') ? (kto[6]-'0')*2 : (kto[6]-'0')*2-9)
            +  (kto[7]-'0')
            + ((kto[8]<'5') ? (kto[8]-'0')*2 : (kto[8]-'0')*2-9);
#else
         pz=(kto[1]-'0')+(kto[3]-'0')+(kto[5]-'0')+(kto[7]-'0');
         if(kto[0]<'5')pz+=pz1=(kto[0]-'0')*2; else pz+=pz1=(kto[0]-'0')*2-9;
         if(kto[2]<'5')pz+=pz1=(kto[2]-'0')*2; else pz+=pz1=(kto[2]-'0')*2-9;
         if(kto[4]<'5')pz+=pz1=(kto[4]-'0')*2; else pz+=pz1=(kto[4]-'0')*2-9;
         if(kto[6]<'5')pz+=pz1=(kto[6]-'0')*2; else pz+=pz1=(kto[6]-'0')*2-9;
         if(kto[8]<'5')pz+=pz1=(kto[8]-'0')*2; else pz+=pz1=(kto[8]-'0')*2-9;
#endif
         pz1=pz;  /* Summe merken für Fall b */
         MOD_10_80;   /* pz%=10 */
         if(pz)pz=10-pz;
         CHECK_PZX10;

         if(*kto=='0' && *(kto+1)=='0' && *(kto+2)=='0' && *(kto+3)=='0' && *(kto+4)!='0'){

#if DEBUG>0
      case 2074:
         if(retvals){
            retvals->methode="74b";
            retvals->pz_methode=2074;
         }
#endif
#if DEBUG>0
            if(untermethode){

                  /* pz wurde noch nicht berechnet; jetzt erledigen.
                   * Da dieser Code nur im DEBUG-Fall auftritt, wurde er
                   * für VMS nicht optimiert.
                   */
               pz1=(kto[5]-'0')+(kto[7]-'0');
               if(kto[4]<'5')pz1+=(kto[4]-'0')*2; else pz1+=(kto[4]-'0')*2-9;
               if(kto[6]<'5')pz1+=(kto[6]-'0')*2; else pz1+=(kto[6]-'0')*2-9;
               if(kto[8]<'5')pz1+=(kto[8]-'0')*2; else pz1+=(kto[8]-'0')*2-9;
            }
#endif
               /* für 6-stellige Kontonummern hochrechnen auf die nächste
                * Halbdekade. Diese Version benutzt nur noch "echte"
                * Halbdekaden (5,15,25...); die "unechten" Halbdekaden 10,20,30
                * wurden schon beim ersten Test in Methode 74a getestet. Für
                * den Test reicht es, zu der alten Zwischensumme (die bereits
                * zur unechten Halbdekade getestet wurde) 5 zu addieren (bei
                * einer Zwischensumme<5) bzw. zu subtrahieren (bei einer
                * Zwischensumme>=5) und den Wert mit der Soll-Prüfziffer zu
                * vergleichen. Mit dieser Methode kommt man jeweils zur
                * "nächsten" Halbdekade.
                *
                * Es bleibt allerdings noch die Frage, ob es für die Methode
                * 74b auch Prüfziffern <5 gibt; mit der angegebenen
                * Berechnungsmethode werden diese als falsch zurückgewiesen,
                * was allerdings auch mit dem Verhalten von anderen Programmen
                * übereinstimmt.
                */
            pz=pz1;
            if(pz<5)
               pz+=5;
            else
               pz-=5;
            MOD_10_80;
            if(pz)pz=10-pz;
            CHECK_PZ10;
         }
         else
            return FALSE;

/*  Berechnung nach der Methode 75 +§§§4 */
/*
 * ######################################################################
 * #              Berechnung nach der Methode 75                        #
 * ######################################################################
 * # Modulus 10, Gewichtung 2, 1, 2, 1, 2.                              #
 * # Die Kontonummer (6-, 7- oder 9-stellig) ist durch linksbündige     #
 * # Nullenauffüllung 10-stellig darzustellen. Die für die Berech-      #
 * # nung relevante 5-stellige Stammnummer wird von links nach          #
 * # rechts mit den Ziffern 2, 1, 2, 1, 2 multipliziert. Die weitere    #
 * # Berechnung und die Ergebnisse entsprechen dem Verfahren 00.        #
 * # Bei 6- und 7-stelligen Kontonummern befindet sich die für die      #
 * # Berechnung relevante Stammnummer in den Stellen 5 bis 9, die       #
 * # Prüfziffer in Stelle 10 der Kontonummer.                           #
 * # Bei 9-stelligen Kontonummern befindet sich die für die Berech-     #
 * # nung relevante Stammnummer in den Stellen 2 bis 6, die Prüf-       #
 * # ziffer in der 7. Stelle der Kontonummer. Ist die erste Stelle      #
 * # der 9-stelligen Kontonummer = 9 (2. Stelle der "gedachten"         #
 * # Kontonummer), so befindet sich die für die Berechnung relevante    #
 * # Stammnummer in den Stellen 3 bis 7, die Prüfziffer in der 8.       #
 * # Stelle der Kontonummer.                                            #
 * ######################################################################
 */
      case 75:
#if DEBUG>0
         if(retvals){
            retvals->methode="75";
            retvals->pz_methode=75;
         }
#endif
         if(*kto!='0')return INVALID_KTO;   /* 10-stellige Kontonummer */
         if(*kto=='0' && *(kto+1)=='0'){   /* 6/7-stellige Kontonummer */
#if DEBUG>0
      case 1075:
         if(retvals){
            retvals->methode="75a";
            retvals->pz_methode=1075;
         }
#endif
            if(*(kto+2)!='0' || (*(kto+2)=='0' && *(kto+3)=='0' && *(kto+4)=='0'))
               return INVALID_KTO;   /* 8- oder <6-stellige Kontonummer */
#ifdef __ALPHA
            pz = ((kto[4]<'5') ? (kto[4]-'0')*2 : (kto[4]-'0')*2-9)
               +  (kto[5]-'0')
               + ((kto[6]<'5') ? (kto[6]-'0')*2 : (kto[6]-'0')*2-9)
               +  (kto[7]-'0')
               + ((kto[8]<'5') ? (kto[8]-'0')*2 : (kto[8]-'0')*2-9);
#else
            pz=(kto[5]-'0')+(kto[7]-'0');
            if(kto[4]<'5')pz+=(kto[4]-'0')*2; else pz+=(kto[4]-'0')*2-9;
            if(kto[6]<'5')pz+=(kto[6]-'0')*2; else pz+=(kto[6]-'0')*2-9;
            if(kto[8]<'5')pz+=(kto[8]-'0')*2; else pz+=(kto[8]-'0')*2-9;
#endif
         MOD_10_80;   /* pz%=10 */
            if(pz)pz=10-pz;
            CHECK_PZ10;
         }
         else if(*(kto+1)=='9'){   /* 9-stellige Kontonummer, Variante 2 */
#if DEBUG>0
      case 2075:
         if(retvals){
            retvals->methode="75b";
            retvals->pz_methode=2075;
         }
#endif
#ifdef __ALPHA
            pz = ((kto[2]<'5') ? (kto[2]-'0')*2 : (kto[2]-'0')*2-9)
               +  (kto[3]-'0')
               + ((kto[4]<'5') ? (kto[4]-'0')*2 : (kto[4]-'0')*2-9)
               +  (kto[5]-'0')
               + ((kto[6]<'5') ? (kto[6]-'0')*2 : (kto[6]-'0')*2-9);
#else
            pz=(kto[3]-'0')+(kto[5]-'0');
            if(kto[2]<'5')pz+=(kto[2]-'0')*2; else pz+=(kto[2]-'0')*2-9;
            if(kto[4]<'5')pz+=(kto[4]-'0')*2; else pz+=(kto[4]-'0')*2-9;
            if(kto[6]<'5')pz+=(kto[6]-'0')*2; else pz+=(kto[6]-'0')*2-9;
#endif
         MOD_10_40;   /* pz%=10 */
            if(pz)pz=10-pz;
            CHECK_PZ8;
         }
         else{   /* 9-stellige Kontonummer, Variante 1 */
#if DEBUG>0
      case 3075:
         if(retvals){
            retvals->methode="75c";
            retvals->pz_methode=3075;
         }
#endif
#ifdef __ALPHA
            pz = ((kto[1]<'5') ? (kto[1]-'0')*2 : (kto[1]-'0')*2-9)
               +  (kto[2]-'0')
               + ((kto[3]<'5') ? (kto[3]-'0')*2 : (kto[3]-'0')*2-9)
               +  (kto[4]-'0')
               + ((kto[5]<'5') ? (kto[5]-'0')*2 : (kto[5]-'0')*2-9);
#else
            pz=(kto[2]-'0')+(kto[4]-'0');
            if(kto[1]<'5')pz+=(kto[1]-'0')*2; else pz+=(kto[1]-'0')*2-9;
            if(kto[3]<'5')pz+=(kto[3]-'0')*2; else pz+=(kto[3]-'0')*2-9;
            if(kto[5]<'5')pz+=(kto[5]-'0')*2; else pz+=(kto[5]-'0')*2-9;
#endif
         MOD_10_40;   /* pz%=10 */
            if(pz)pz=10-pz;
            CHECK_PZ7;
         }

/*  Berechnung nach der Methode 76 +§§§4 */
/*
 * ######################################################################
 * #              Berechnung nach der Methode 76                        #
 * ######################################################################
 * # Modulus 11, Gewichtung 2, 3, 4, 5 ff.                              #
 * # Die einzelnen Stellen der für die Berechnung der Prüfziffer        #
 * # relevanten 5-, 6- oder 7-stelligen Stammnummer sind von rechts     #
 * # nach links mit den Ziffern 2, 3, 4, 5 ff. zu multiplizieren.       #
 * # Die jeweiligen Produkte werden addiert. Die Summe ist durch 11     #
 * # zu dividieren. Der verbleibende Rest ist die Prüfziffer. Ist       #
 * # der Rest 10, kann die Kontonummer nicht verwendet werden.          #
 * # Darstellung der Kontonummer: ASSSSSSPUU.                           #
 * # Ist die Unterkontonummer "00", kann es vorkommen, daß sie auf      #
 * # Zahlungsbelegen nicht angegeben ist. Die Prüfziffer ist dann       #
 * # an die 10. Stelle gerückt.                                         #
 * # Die Kontoart (1. Stelle) kann den Wert 0, 4, 6, 7, 8 oder 9 haben. #
 * ######################################################################
 */
      case 76:
#if DEBUG>0
      case 1076:
         if(retvals){
            retvals->methode="76a";
            retvals->pz_methode=1076;
         }
#endif
         if((p1= *kto)=='1' || p1=='2' || p1=='3' || p1=='5'){
            pz= -3;
            return INVALID_KTO;
         }
         pz = (kto[1]-'0') * 7
            + (kto[2]-'0') * 6
            + (kto[3]-'0') * 5
            + (kto[4]-'0') * 4
            + (kto[5]-'0') * 3
            + (kto[6]-'0') * 2;

         MOD_11_176;   /* pz%=11 */
         CHECK_PZX8;

#if DEBUG>0
      case 2076:
         if(retvals){
            retvals->methode="76b";
            retvals->pz_methode=2076;
         }
#endif
         if((p1=kto[2])=='1' || p1=='2' || p1=='3' || p1=='5'){
#if DEBUG>0
            pz= -3;
#endif
            return INVALID_KTO;
         }
         pz = (kto[3]-'0') * 7
            + (kto[4]-'0') * 6
            + (kto[5]-'0') * 5
            + (kto[6]-'0') * 4
            + (kto[7]-'0') * 3
            + (kto[8]-'0') * 2;

         MOD_11_176;   /* pz%=11 */
         if(pz==10){
            pz= -2;
            return INVALID_KTO;
         }
         CHECK_PZ10;

/*  Berechnung nach der Methode 77 +§§§4 */
/*
 * ######################################################################
 * #              Berechnung nach der Methode 77                        #
 * ######################################################################
 * # Modulus 11, Gewichtung 1, 2, 3, 4, 5.                              #
 * # Die Kontonummer ist 10-stellig. Die für die Berechnung             #
 * # relevanten Stellen 6 bis 10 werden von rechts nach links mit       #
 * # den Ziffern 1, 2, 3, 4, 5 multipliziert. Die Produkte werden       #
 * # addiert. Die Summe ist durch 11 zu dividieren. Verbleibt nach      #
 * # der Division der Summe durch 11 ein Rest, ist folgende neue        #
 * # Berechnung durchzuführen:                                          #
 * # Modulus 11, Gewichtung 5, 4, 3, 4, 5.                              #
 * # Ergibt sich bei der erneuten Berechnung wiederum ein Rest,         #
 * # dann ist die Kontonummer falsch.                                   #
 * ######################################################################
 */
      case 77:
#if DEBUG>0
      case 1077:
         if(retvals){
            retvals->methode="77a";
            retvals->pz_methode=1077;
         }
#endif
         pz = (kto[5]-'0') * 5
            + (kto[6]-'0') * 4
            + (kto[7]-'0') * 3
            + (kto[8]-'0') * 2
            + (kto[9]-'0');

         MOD_11_88;   /* pz%=11 */
         if(pz==0)return OK;
#if DEBUG>0
         if(untermethode)return INVALID_KTO;
#endif
#if DEBUG>0
      case 2077:
         if(retvals){
            retvals->methode="77b";
            retvals->pz_methode=2077;
         }
#endif
         pz = (kto[5]-'0') * 5
            + (kto[6]-'0') * 4
            + (kto[7]-'0') * 3
            + (kto[8]-'0') * 4
            + (kto[9]-'0') * 5;

         MOD_11_176;   /* pz%=11 */
         if(pz==0)
            return OK;
         else
#if DEBUG>0
         if(untermethode)
            return INVALID_KTO;
         else
#endif
            return FALSE;

/*  Berechnung nach der Methode 78 +§§§4 */
/*
 * ######################################################################
 * #              Berechnung nach der Methode 78                        #
 * ######################################################################
 * # Modulus 10, Gewichtung 2, 1, 2, 1, 2, 1, 2, 1, 2.                  #
 * # Die Berechnung erfolgt wie bei Verfahren 00. Ausnahme:             #
 * # 8-stellige Kontonummern sind nicht prüfbar, da diese Nummern       #
 * # keine Prüfziffer enthalten.                                        #
 * ######################################################################
 */
      case 78:
#if DEBUG>0
         if(retvals){
            retvals->methode="78";
            retvals->pz_methode=78;
         }
#endif
         if(*kto=='0' && *(kto+1)=='0' && *(kto+2)!='0')
#if BAV_KOMPATIBEL
            return BAV_FALSE;
#else
            return OK_NO_CHK;
#endif
#ifdef __ALPHA
         pz = ((kto[0]<'5') ? (kto[0]-'0')*2 : (kto[0]-'0')*2-9)
            +  (kto[1]-'0')
            + ((kto[2]<'5') ? (kto[2]-'0')*2 : (kto[2]-'0')*2-9)
            +  (kto[3]-'0')
            + ((kto[4]<'5') ? (kto[4]-'0')*2 : (kto[4]-'0')*2-9)
            +  (kto[5]-'0')
            + ((kto[6]<'5') ? (kto[6]-'0')*2 : (kto[6]-'0')*2-9)
            +  (kto[7]-'0')
            + ((kto[8]<'5') ? (kto[8]-'0')*2 : (kto[8]-'0')*2-9);
#else
         pz =(kto[1]-'0')+(kto[3]-'0')+(kto[5]-'0')+(kto[7]-'0');
         if(kto[0]<'5')pz+=(kto[0]-'0')*2; else pz+=(kto[0]-'0')*2-9;
         if(kto[2]<'5')pz+=(kto[2]-'0')*2; else pz+=(kto[2]-'0')*2-9;
         if(kto[4]<'5')pz+=(kto[4]-'0')*2; else pz+=(kto[4]-'0')*2-9;
         if(kto[6]<'5')pz+=(kto[6]-'0')*2; else pz+=(kto[6]-'0')*2-9;
         if(kto[8]<'5')pz+=(kto[8]-'0')*2; else pz+=(kto[8]-'0')*2-9;
#endif

         MOD_10_80;   /* pz%=10 */
         if(pz)pz=10-pz;
         CHECK_PZ10;

/*  Berechnung nach der Methode 79 +§§§4 */
/*
 * ######################################################################
 * #              Berechnung nach der Methode 79                        #
 * ######################################################################
 * # Modulus 10, Gewichtung 2, 1, 2, 1, 2, 1, 2 ff.                     #
 * # Die Kontonummer ist 10-stellig. Die Berechnung und Ergebnisse      #
 * # entsprechen dem Verfahren 00. Es ist jedoch zu beachten, dass      #
 * # die Berechnung vom Wert der 1. Stelle der Kontonummer abhängig     #
 * # ist.                                                               #
 * #                                                                    #
 * # Variante 1:                                                        #
 * # Die 1. Stelle der Kontonummer hat die Ziffer 3, 4, 5, 6, 7         #
 * # oder 8                                                             #
 * # Die für die Berechnung relevanten Stellen der Kontonummer          #
 * # befinden sich in den Stellen 1 bis 9. Die 10. Stelle ist per       #
 * # Definition die Prüfziffer.                                         #
 * #                                                                    #
 * # Variante 2:                                                        #
 * # Die 1. Stelle der Kontonummer hat die Ziffer 1, 2 oder 9           #
 * # Die für die Berechnung relevanten Stellen der Kontonummer          #
 * # befinden sich in den Stellen 1 bis 8. Die 9. Stelle ist die        #
 * # Prüfziffer der 10-stelligen Kontonummer.                           #
 * #                                                                    #
 * # Kontonummern, die in der 1. Stelle eine 0 haben,                   #
 * # wurden nicht vergeben und gelten deshalb als falsch.               #
 * ######################################################################
 */
      case 79:
#if DEBUG>0
         if(retvals){
            retvals->methode="79";
            retvals->pz_methode=79;
         }
#endif
         if(*kto=='0')return INVALID_KTO;
         if(*kto=='1' || *kto=='2' || *kto=='9'){
#ifdef __ALPHA
         pz =  (kto[0]-'0')
            + ((kto[1]<'5') ? (kto[1]-'0')*2 : (kto[1]-'0')*2-9)
            +  (kto[2]-'0')
            + ((kto[3]<'5') ? (kto[3]-'0')*2 : (kto[3]-'0')*2-9)
            +  (kto[4]-'0')
            + ((kto[5]<'5') ? (kto[5]-'0')*2 : (kto[5]-'0')*2-9)
            +  (kto[6]-'0')
            + ((kto[7]<'5') ? (kto[7]-'0')*2 : (kto[7]-'0')*2-9);
#else
         pz =(kto[0]-'0')+(kto[2]-'0')+(kto[4]-'0')+(kto[6]-'0');
         if(kto[1]<'5')pz+=(kto[1]-'0')*2; else pz+=(kto[1]-'0')*2-9;
         if(kto[3]<'5')pz+=(kto[3]-'0')*2; else pz+=(kto[3]-'0')*2-9;
         if(kto[5]<'5')pz+=(kto[5]-'0')*2; else pz+=(kto[5]-'0')*2-9;
         if(kto[7]<'5')pz+=(kto[7]-'0')*2; else pz+=(kto[7]-'0')*2-9;
#endif

            MOD_10_80;   /* pz%=10 */
            if(pz)pz=10-pz;
            CHECK_PZ9;
         }
         else{
#ifdef __ALPHA
            pz = ((kto[0]<'5') ? (kto[0]-'0')*2 : (kto[0]-'0')*2-9)
               +  (kto[1]-'0')
               + ((kto[2]<'5') ? (kto[2]-'0')*2 : (kto[2]-'0')*2-9)
               +  (kto[3]-'0')
               + ((kto[4]<'5') ? (kto[4]-'0')*2 : (kto[4]-'0')*2-9)
               +  (kto[5]-'0')
               + ((kto[6]<'5') ? (kto[6]-'0')*2 : (kto[6]-'0')*2-9)
               +  (kto[7]-'0')
               + ((kto[8]<'5') ? (kto[8]-'0')*2 : (kto[8]-'0')*2-9);
#else
            pz =(kto[1]-'0')+(kto[3]-'0')+(kto[5]-'0')+(kto[7]-'0');
            if(kto[0]<'5')pz+=(kto[0]-'0')*2; else pz+=(kto[0]-'0')*2-9;
            if(kto[2]<'5')pz+=(kto[2]-'0')*2; else pz+=(kto[2]-'0')*2-9;
            if(kto[4]<'5')pz+=(kto[4]-'0')*2; else pz+=(kto[4]-'0')*2-9;
            if(kto[6]<'5')pz+=(kto[6]-'0')*2; else pz+=(kto[6]-'0')*2-9;
            if(kto[8]<'5')pz+=(kto[8]-'0')*2; else pz+=(kto[8]-'0')*2-9;
#endif

            MOD_10_80;   /* pz%=10 */
            if(pz)pz=10-pz;
            CHECK_PZ10;
         }

/* Berechnungsmethoden 80 bis 89 +§§§3
   Berechnung nach der Methode 80 +§§§4 */
/*
 * ######################################################################
 * #    Berechnung nach der Methode 80 (geändert zum 8.6.2004)          #
 * ######################################################################
 * # Modulus 10, Gewichtung 2, 1, 2, 1, 2.                              #
 * # Die Berechnung und die möglichen Ergebnisse entsprechen dem        #
 * # Verfahren 00; es ist jedoch  zu beachten, daß nur die Stellen      #
 * # 5 bis 9 in das Prüfziffernberechnungsverfahren einbezogen          #
 * # werden.                                                            #
 * # Führt die Berechnung zu einem Prüfziffernfehler, so ist die        #
 * # Berechnung nach Variante 2 vorzunehmen. Das Berechnungsverfahren   #
 * # entspricht Variante 1. Die Summe der Produkte ist jedoch durch     #
 * # 7 zu dividieren. Der verbleibende Rest wird vom Divisor (7)        #
 * # subtrahiert. Das Ergebnis ist die Prüfziffer. Verbleibt nach       #
 * # der Division kein Rest, ist die Prüfziffer 0.                      #
 * #                                                                    #
 * # Ausnahme (alt, als Referenz):                                      #
 * # Sind die 3. und 4. Stelle der Kontonummer = 99, so erfolgt die     #
 * # Berechnung nach Verfahren 10.                                      #
 * #                                                                    #
 * # Ausnahme (neu ab 8.6.04):                                          #
 * # Ist nach linksbündiger Auffüllung mit Nullen auf 10 Stellen die    #
 * # 3. Stelle der Kontonummer = 9 (Sachkonten), so erfolgt die         #
 * # Berechnung gemäß der Ausnahme in Methode 51 mit den gleichen       #
 * # Ergebnissen und Testkontonummern.                                  #
 * ######################################################################
 */

      case 80:

                /* Ausnahme, Variante 1 */
         if(*(kto+2)=='9'){   /* Berechnung wie in Verfahren 51 */
#if DEBUG>0
      case 3080:
         if(retvals){
            retvals->methode="80c";
            retvals->pz_methode=3080;
         }
#endif
            pz =         9 * 8
            + (kto[3]-'0') * 7
            + (kto[4]-'0') * 6
            + (kto[5]-'0') * 5
            + (kto[6]-'0') * 4
            + (kto[7]-'0') * 3
            + (kto[8]-'0') * 2;

            MOD_11_176;   /* pz%=11 */
            if(pz<=1)
               pz=0;
            else
               pz=11-pz;
            CHECK_PZX10;

            /* Ausnahme, Variante 2 */
#if DEBUG>0
      case 4080:
         if(retvals){
            retvals->methode="80d";
            retvals->pz_methode=4080;
         }
#endif
               pz = (kto[0]-'0') * 10
               + (kto[1]-'0') * 9
               +            9 * 8
               + (kto[3]-'0') * 7
               + (kto[4]-'0') * 6
               + (kto[5]-'0') * 5
               + (kto[6]-'0') * 4
               + (kto[7]-'0') * 3
               + (kto[8]-'0') * 2;

               MOD_11_352;   /* pz%=11 */
               if(pz<=1)
                  pz=0;
               else
                  pz=11-pz;
               CHECK_PZ10;
            }

            /* Variante 1 */
#if DEBUG>0
      case 1080:
         if(retvals){
            retvals->methode="80a";
            retvals->pz_methode=1080;
         }
#endif
#ifdef __ALPHA
         pz = ((kto[4]<'5') ? (kto[4]-'0')*2 : (kto[4]-'0')*2-9)
            +  (kto[5]-'0')
            + ((kto[6]<'5') ? (kto[6]-'0')*2 : (kto[6]-'0')*2-9)
            +  (kto[7]-'0')
            + ((kto[8]<'5') ? (kto[8]-'0')*2 : (kto[8]-'0')*2-9);
#else
         pz =(kto[5]-'0')+(kto[7]-'0');
         if(kto[4]<'5')pz+=(kto[4]-'0')*2; else pz+=(kto[4]-'0')*2-9;
         if(kto[6]<'5')pz+=(kto[6]-'0')*2; else pz+=(kto[6]-'0')*2-9;
         if(kto[8]<'5')pz+=(kto[8]-'0')*2; else pz+=(kto[8]-'0')*2-9;
#endif

         MOD_10_40;   /* pz%=10 */
         if(pz)pz=10-pz;
         CHECK_PZX10;

            /* Variante 2 */
#if DEBUG>0
      case 2080:
         if(retvals){
            retvals->methode="80b";
            retvals->pz_methode=2080;
         }
#endif
#ifdef __ALPHA
         pz = ((kto[4]<'5') ? (kto[4]-'0')*2 : (kto[4]-'0')*2-9)
            +  (kto[5]-'0')
            + ((kto[6]<'5') ? (kto[6]-'0')*2 : (kto[6]-'0')*2-9)
            +  (kto[7]-'0')
            + ((kto[8]<'5') ? (kto[8]-'0')*2 : (kto[8]-'0')*2-9);
#else
         pz =(kto[5]-'0')+(kto[7]-'0');
         if(kto[4]<'5')pz+=(kto[4]-'0')*2; else pz+=(kto[4]-'0')*2-9;
         if(kto[6]<'5')pz+=(kto[6]-'0')*2; else pz+=(kto[6]-'0')*2-9;
         if(kto[8]<'5')pz+=(kto[8]-'0')*2; else pz+=(kto[8]-'0')*2-9;
#endif

         MOD_7_56;   /* pz%=10 */
         if(pz)pz=7-pz;
         CHECK_PZ10;

/*  Berechnung nach der Methode 81 +§§§4 */
/*
 * ######################################################################
 * #    Berechnung nach der Methode 81  (geändert zum 6.9.2004)         #
 * ######################################################################
 * #                                                                    #
 * # Modulus 11, Gewichtung 2, 3, 4, 5, 6, 7                            #
 * # Die Kontonummer ist durch linksbündige Nullenauffüllung stellig    #
 * # darzustellen. Die 10. Stelle ist per Definition Prüfziffer. Die    #
 * # für die Berechnung relevanten Stellen werden von rechts nach       #
 * # links mit den Ziffern 2, 3, 4, 5, 6, multipliziert. Die weitere    #
 * # Berechnung und die möglichen Ergebnisse entsprechen dem Verfahren  #
 * # 32.                                                                #
 * #                                                                    #
 * # Ausnahme:                                                          #
 * # Ist nach linksbündiger Auffüllung mit Nullen auf 10 Stellen 3.     #
 * # Stelle der Kontonummer = 9 (Sachkonten), so erfolgt Berechnung     #
 * # gemäß der Ausnahme in Methode 51 mit gleichen Ergebnissen und      #
 * # Testkontonummern.                                                  #
 * ######################################################################
 */

      case 81:
                /* Ausnahme, Variante 1 */
         if(*(kto+2)=='9'){   /* Berechnung wie in Verfahren 51 */
#if DEBUG>0
      case 2081:
         if(retvals){
            retvals->methode="81b";
            retvals->pz_methode=2081;
         }
#endif
            pz =         9 * 8
            + (kto[3]-'0') * 7
            + (kto[4]-'0') * 6
            + (kto[5]-'0') * 5
            + (kto[6]-'0') * 4
            + (kto[7]-'0') * 3
            + (kto[8]-'0') * 2;

            MOD_11_176;   /* pz%=11 */
            if(pz<=1)
               pz=0;
            else
               pz=11-pz;
            CHECK_PZX10;

            /* Ausnahme, Variante 2 */
#if DEBUG>0
      case 3081:
         if(retvals){
            retvals->methode="81c";
            retvals->pz_methode=3081;
         }
#endif
               pz = (kto[0]-'0') * 10
               + (kto[1]-'0') * 9
               +            9 * 8
               + (kto[3]-'0') * 7
               + (kto[4]-'0') * 6
               + (kto[5]-'0') * 5
               + (kto[6]-'0') * 4
               + (kto[7]-'0') * 3
               + (kto[8]-'0') * 2;

               MOD_11_352;   /* pz%=11 */
               if(pz<=1)
                  pz=0;
               else
                  pz=11-pz;
               CHECK_PZ10;
            }

#if DEBUG>0
      case 1081:
         if(retvals){
            retvals->methode="81a";
            retvals->pz_methode=1081;
         }
#endif
         pz = (kto[3]-'0') * 7
            + (kto[4]-'0') * 6
            + (kto[5]-'0') * 5
            + (kto[6]-'0') * 4
            + (kto[7]-'0') * 3
            + (kto[8]-'0') * 2;

         MOD_11_176;   /* pz%=11 */
         if(pz<=1)
            pz=0;
         else
            pz=11-pz;
         CHECK_PZ10;

/*  Berechnung nach der Methode 82 +§§§4 */
/*
 * ######################################################################
 * #              Berechnung nach der Methode 82                        #
 * ######################################################################
 * # Modulus 11, Gewichtung 2, 3, 4, 5, 6, 7.                           #
 * # Sind die 3. und 4. Stelle der Kontonummer = 99, so erfolgt die     #
 * # Berechnung nach Verfahren 10, sonst nach Verfahren 33.             #
 * ######################################################################
 */
      case 82:
#if DEBUG>0
      case 1082:
         if(retvals){
            retvals->methode="82a";
            retvals->pz_methode=1082;
         }
#endif
            /* Verfahren 10 */
         if(*(kto+2)=='9' && *(kto+3)=='9'){
            pz = (kto[0]-'0') * 10
               + (kto[1]-'0') * 9
               + (kto[2]-'0') * 8
               + (kto[3]-'0') * 7
               + (kto[4]-'0') * 6
               + (kto[5]-'0') * 5
               + (kto[6]-'0') * 4
               + (kto[7]-'0') * 3
               + (kto[8]-'0') * 2;

            MOD_11_352;   /* pz%=11 */
            if(pz<=1)
               pz=0;
            else
               pz=11-pz;
            CHECK_PZ10;
         }

            /* Verfahren 33 */
#if DEBUG>0
      case 2082:
         if(retvals){
            retvals->methode="82b";
            retvals->pz_methode=2082;
         }
#endif
         pz = (kto[4]-'0') * 6
            + (kto[5]-'0') * 5
            + (kto[6]-'0') * 4
            + (kto[7]-'0') * 3
            + (kto[8]-'0') * 2;

         MOD_11_176;   /* pz%=11 */
         if(pz<=1)
            pz=0;
         else
            pz=11-pz;
         CHECK_PZ10;

/*  Berechnung nach der Methode 83 +§§§4 */
/*
 * ######################################################################
 * #              Berechnung nach der Methode 83                        #
 * ######################################################################
 * # 1. Kundenkonten                                                    #
 * # A. Modulus 11, Gewichtung 2, 3, 4, 5, 6, 7                         #
 * # B. Modulus 11, Gewichtung 2, 3, 4, 5, 6                            #
 * # C. Modulus  7, Gewichtung 2, 3, 4, 5, 6                            #
 * # Gemeinsame Anmerkungen für die Berechnungsverfahren:               #
 * # Die Kontonummer ist immer 10-stellig. Die für die Berechnung       #
 * # relevante Kundennummer (K) befindet sich bei der Methode A in      #
 * # den Stellen 4 bis 9 der Kontonummer und bei den Methoden B + C     #
 * # in den Stellen 5 - 9, die Prüfziffer in Stelle 10 der              #
 * # Kontonummer.                                                       #
 * #                                                                    #
 * # Ergibt die erste Berechnung der Prüfziffer nach dem Verfahren A    #
 * # einen Prüfzifferfehler, so sind weitere Berechnungen mit den       #
 * # anderen Methoden vorzunehmen. Kontonummern, die nach               #
 * # Durchführung aller 3 Berechnungsmethoden nicht zu einem            #
 * # richtigen Ergebnis führen, sind nicht prüfbar.                     #
 * #                                                                    #
 * # Methode A:                                                         #
 * # Modulus 11, Gewichtung 2, 3, 4, 5, 6, 7                            #
 * # Die Berechnung und möglichen Ergebnisse entsprechen                #
 * # dem Verfahren 32.                                                  #
 * #                                                                    #
 * # Methode B:                                                         #
 * # Modulus 11, Gewichtung 2, 3, 4, 5, 6                               #
 * # Die Berechnung und möglichen Ergebnisse entsprechen                #
 * # dem Verfahren 33.                                                  #
 * #                                                                    #
 * # Methode C:                                                         #
 * # Kontonummern, die bis zur Methode C gelangen und in der 10.        #
 * # Stelle eine 7, 8 oder 9 haben, sind ungültig. Modulus 7,           #
 * # Gewichtung 2, 3, 4, 5, 6 Das Berechnungsverfahren entspricht       #
 * # Methode B. Die Summe der Produkte ist jedoch durch 7 zu            #
 * # dividieren. Der verbleibende Rest wird vom Divisor (7)             #
 * # subtrahiert. Das Ergebnis ist die Prüfziffer. Verbleibt kein       #
 * # Rest, ist die Prüfziffer 0.                                        #
 * #                                                                    #
 * # 2. Sachkonten                                                      #
 * # Berechnungsmethode:                                                #
 * # Modulus 11, Gewichtung 2, 3, 4, 5, 6, 7, 8                         #
 * # Die Sachkontonummer ist immer 10-stellig. Die für die Berechnung   #
 * # relevante  Sachkontontenstammnummer (S) befindet sich in den       #
 * # Stellen 3 bis 9 der Kontonummer, wobei die 3. und 4. Stelle        #
 * # immer jeweils 9 sein müssen; die Prüfziffer ist in Stelle 10 der   #
 * # Sachkontonummer. Führt die Berechnung nicht zu einem richtigen     #
 * # Ergebnis, ist die Nummer nicht prüfbar.                            #
 * # Berechnung:                                                        #
 * # Die einzelnen Stellen der Sachkontonummern sind von rechts nach    #
 * # links mit den Ziffern 2, 3, 4, 5, 6, 7, 8 zu multiplizieren. Die   #
 * # jeweiligen Produkte werden addiert. Die Summe ist durch 11 zu      #
 * # dividieren. Der verbleibende Rest wird vom Divisor (11)            #
 * # subtrahiert. Das Ergebnis ist die Prüfziffer. Verbleibt nach der   #
 * # Division durch die 11 kein Rest, ist die Prüfziffer "0". Das       #
 * # Rechenergebnis "10" ist nicht verwendbar und muss auf eine         #
 * # Stelle reduziert werden. Die rechte Stelle Null findet als         #
 * # Prüfziffer Verwendung.                                             #
 * ######################################################################
 */
      case 83:
            /* Sachkonten */
#if DEBUG>0
      case 4083:
         if(retvals){
            retvals->methode="83d";
            retvals->pz_methode=4083;
         }
#endif
         if(*(kto+2)=='9' && *(kto+3)=='9'){
            pz =      9       * 8
               +      9       * 7
               + (kto[4]-'0') * 6
               + (kto[5]-'0') * 5
               + (kto[6]-'0') * 4
               + (kto[7]-'0') * 3
               + (kto[8]-'0') * 2;

            MOD_11_176;   /* pz%=11 */
            if(pz<=1)
               pz=0;
            else
               pz=11-pz;
            CHECK_PZ10;
         }

            /* Methode A */
#if DEBUG>0
      case 1083:
         if(retvals){
            retvals->methode="83a";
            retvals->pz_methode=1083;
         }
#endif
         pz = (kto[3]-'0') * 7
            + (kto[4]-'0') * 6
            + (kto[5]-'0') * 5
            + (kto[6]-'0') * 4
            + (kto[7]-'0') * 3
            + (kto[8]-'0') * 2;

         MOD_11_176;   /* pz%=11 */
         if(pz<=1)
            pz=0;
         else
            pz=11-pz;
         CHECK_PZX10;

            /* Methode B */
#if DEBUG>0
      case 2083:
         if(retvals){
            retvals->methode="83b";
            retvals->pz_methode=2083;
         }
#endif
         pz = (kto[4]-'0') * 6
            + (kto[5]-'0') * 5
            + (kto[6]-'0') * 4
            + (kto[7]-'0') * 3
            + (kto[8]-'0') * 2;

         MOD_11_176;   /* pz%=11 */
         if(pz<=1)
            pz=0;
         else
            pz=11-pz;
         CHECK_PZX10;

            /* Methode C */
#if DEBUG>0
      case 3083:
         if(retvals){
            retvals->methode="83c";
            retvals->pz_methode=3083;
         }
#endif
         pz = (kto[4]-'0') * 6
            + (kto[5]-'0') * 5
            + (kto[6]-'0') * 4
            + (kto[7]-'0') * 3
            + (kto[8]-'0') * 2;

         MOD_7_112;   /* pz%=7 */
         if(pz)pz=7-pz;
         CHECK_PZ10;

/*  Berechnung nach der Methode 84 +§§§4 */
/*
 * ######################################################################
 * #          Berechnung nach der Methode 84 (geändert zum 6.9.04)      #
 * ######################################################################
 * # Modulus 11, Gewichtung 2, 3, 4, 5, 6                               #
 * # Variante 1                                                         #
 * # Die Kontonummer ist durch linksbündige Nullenauffüllung            #
 * # 10-stellig darzustellen. Die 10. Stelle ist per Definition die     #
 * # Prüfziffer. Die für die Berechnung relevanten Stellen werden       #
 * # von rechts nach links mit den Ziffern 2, 3, 4, 5, 6                #
 * # multipliziert. Die weitere Berechnung und die möglichen            #
 * # Ergebnisse entsprechen dem Verfahren 33.                           #
 * # Führt die Berechnung nach Variante 1 zu einem Prüfziffer-          #
 * # fehler, ist die Berechnung nach Variante 2 vorzunehmen.            #
 * # Variante 2                                                         #
 * # Modulus 7, Gewichtung 2, 3, 4, 5, 6                                #
 * # Die Stellen 5 bis 9 der Kontonummer werden von rechts              #
 * # nach links mit den Gewichten multipliziert. Die jeweiligen         #
 * # Produkte werden addiert. Die Summe ist durch 7 zu                  #
 * # dividieren. Der verbleibende Rest wird vom Divisor (7)             #
 * # subtrahiert. Das Ergebnis ist die Prüfziffer. Verbleibt nach       #
 * # der Division kein Rest, ist die Prüfziffer = 0.                    #
 * #                                                                    #
 * # Ausnahme (neu ab 6.9.04):                                          #
 * # Ist nach linksbündiger Auffüllung mit Nullen auf 10 Stellen die    #
 * # 3. Stelle der Kontonummer = 9 (Sachkonten), so erfolgt die         #
 * # Berechnung gemäß der Ausnahme in Methode 51 mit den gleichen       #
 * # Ergebnissen und Testkontonummern.                                  #
 * ######################################################################
 */

      case 84:

                /* Ausnahme, Variante 1 */
         if(*(kto+2)=='9'){   /* Berechnung wie in Verfahren 51 */
#if DEBUG>0
      case 3084:
         if(retvals){
            retvals->methode="84c";
            retvals->pz_methode=3084;
         }
#endif
            pz =         9 * 8
            + (kto[3]-'0') * 7
            + (kto[4]-'0') * 6
            + (kto[5]-'0') * 5
            + (kto[6]-'0') * 4
            + (kto[7]-'0') * 3
            + (kto[8]-'0') * 2;

            MOD_11_176;   /* pz%=11 */
            if(pz<=1)
               pz=0;
            else
               pz=11-pz;
            CHECK_PZX10;

            /* Ausnahme, Variante 2 */
#if DEBUG>0
      case 4084:
         if(retvals){
            retvals->methode="84d";
            retvals->pz_methode=4084;
         }
#endif
               pz = (kto[0]-'0') * 10
               + (kto[1]-'0') * 9
               +            9 * 8
               + (kto[3]-'0') * 7
               + (kto[4]-'0') * 6
               + (kto[5]-'0') * 5
               + (kto[6]-'0') * 4
               + (kto[7]-'0') * 3
               + (kto[8]-'0') * 2;

               MOD_11_352;   /* pz%=11 */
               if(pz<=1)
                  pz=0;
               else
                  pz=11-pz;
               CHECK_PZ10;
            }

            /* Variante 1 */
#if DEBUG>0
      case 1084:
         if(retvals){
            retvals->methode="84a";
            retvals->pz_methode=1084;
         }
#endif
         pz = (kto[4]-'0') * 6
            + (kto[5]-'0') * 5
            + (kto[6]-'0') * 4
            + (kto[7]-'0') * 3
            + (kto[8]-'0') * 2;

         MOD_11_176;   /* pz%=11 */
         if(pz<=1)
            pz=0;
         else
            pz=11-pz;
         CHECK_PZX10;

            /* Variante 2 */
#if DEBUG>0
      case 2084:
         if(retvals){
            retvals->methode="84b";
            retvals->pz_methode=2084;
         }
#endif
         pz = (kto[4]-'0') * 6
            + (kto[5]-'0') * 5
            + (kto[6]-'0') * 4
            + (kto[7]-'0') * 3
            + (kto[8]-'0') * 2;

         MOD_7_112;   /* pz%=7 */
         if(pz)pz=7-pz;
         CHECK_PZ10;

/*  Berechnung nach der Methode 85 +§§§4 */
/*
 * ######################################################################
 * #              Berechnung nach der Methode 85                        #
 * ######################################################################
 * # Modulus 11, Gewichtung 2, 3, 4, 5, 6.                              #
 * # Wie Verfahren 83, jedoch folgende Ausnahme:                        #
 * # Sind die 3. und 4. Stelle der Kontonummer = 99, so ist folgende    #
 * # Prüfziffernberechnung maßgebend:                                   #
 * # Modulus 11, Gewichtung 2, 3, 4, 5, 6, 7, 8.                        #
 * # Die für die Berechnung relevanten Stellen 3 bis 9 der Kontonr      #
 * # werden von rechts nach links mit den Ziffern 2, 3, 4, 5, 6, 7, 8   #
 * # multipliziert. Die weitere Berechnung und möglichen Ergebnisse     #
 * # entsprechen dem Verfahren 02.                                      #
 * ######################################################################
 */
      case 85:
#if DEBUG>0
      case 4085:
         if(retvals){
            retvals->methode="85d";
            retvals->pz_methode=4085;
         }
#endif
            /* Sachkonten */
         if(*(kto+2)=='9' && *(kto+3)=='9'){
            pz =      9       * 8
               +      9       * 7
               + (kto[4]-'0') * 6
               + (kto[5]-'0') * 5
               + (kto[6]-'0') * 4
               + (kto[7]-'0') * 3
               + (kto[8]-'0') * 2;

            MOD_11_176;   /* pz%=11 */
            if(pz)pz=11-pz;
            INVALID_PZ10;
            CHECK_PZ10;
         }

            /* Methode A */
#if DEBUG>0
      case 1085:
         if(retvals){
            retvals->methode="85a";
            retvals->pz_methode=1085;
         }
#endif
         pz = (kto[3]-'0') * 7
            + (kto[4]-'0') * 6
            + (kto[5]-'0') * 5
            + (kto[6]-'0') * 4
            + (kto[7]-'0') * 3
            + (kto[8]-'0') * 2;

         MOD_11_176;   /* pz%=11 */
         if(pz<=1)
            pz=0;
         else
            pz=11-pz;
         CHECK_PZX10;

            /* Methode B */
#if DEBUG>0
      case 2085:
         if(retvals){
            retvals->methode="85b";
            retvals->pz_methode=2085;
         }
#endif
         pz = (kto[4]-'0') * 6
            + (kto[5]-'0') * 5
            + (kto[6]-'0') * 4
            + (kto[7]-'0') * 3
            + (kto[8]-'0') * 2;

         MOD_11_176;   /* pz%=11 */
         if(pz<=1)
            pz=0;
         else
            pz=11-pz;
         CHECK_PZX10;

            /* Methode C */
#if DEBUG>0
      case 3085:
         if(retvals){
            retvals->methode="85c";
            retvals->pz_methode=3085;
         }
#endif
         pz = (kto[4]-'0') * 6
            + (kto[5]-'0') * 5
            + (kto[6]-'0') * 4
            + (kto[7]-'0') * 3
            + (kto[8]-'0') * 2;

         MOD_7_112;   /* pz%=7 */
         if(pz)pz=7-pz;
         CHECK_PZ10;

/*  Berechnung nach der Methode 86 +§§§4 */
/*
 * ######################################################################
 * #    Berechnung nach der Methode 86 (geändert zum 6.9.2004)          #
 * ######################################################################
 * # Modulus 10, Gewichtung 2, 1, 2, 1, 2, 1                            #
 * #                                                                    #
 * # Methode A                                                          #
 * # Die Kontonummer ist durch linksbündige Nullenauffüllung 10-        #
 * # stellig darzustellen. Die Berechnung und die möglichen             #
 * # Ergebnisse entsprechen dem Verfahren 00; es ist jedoch zu          #
 * # beachten, dass nur die Stellen 4 bis 9 in das                      #
 * # Prüfzifferberechnungsverfahren einbezogen werden. Die Stelle       #
 * # 10 der Kontonummer ist die Prüfziffer.                             #
 * #                                                                    #
 * # Führt die Berechnung nach Methode A zu einem Prüfziffer-           #
 * # fehler, so ist die Berechnung nach Methode B vorzunehmen.          #
 * #                                                                    #
 * # Methode B                                                          #
 * # Modulus 11, Gewichtung 2, 3, 4, 5, 6, 7                            #
 * # Die Kontonummer ist durch linksbündige Nullenauffüllung 10-        #
 * # stellig darzustellen. Die Stellen 4 bis 9 der Kontonummer          #
 * # werden von rechts nach links mit den Ziffern 2, 3, 4, 5, 6, 7      #
 * # multipliziert. Die weitere Berechnung und die möglichen            #
 * # Ergebnisse entsprechen dem Verfahren 32. Die Stelle 10 ist die     #
 * # Prüfziffer.                                                        #
 * #                                                                    #
 * # Ausnahme:                                                          #
 * # Ist nach linksbündiger Auffüllung mit Nullen auf 10 Stellen        #
 * # die 3. Stelle der Kontonummer = 9 (Sachkonten), so erfolgt die     #
 * # Berechnung gemäß der Ausnahme in Methode 51 mit den gleichen       #
 * # Ergebnissen und Testkontonummern.                                  #
 * ######################################################################
 */

      case 86:

                /* Ausnahme, Variante 1 */
         if(*(kto+2)=='9'){   /* Berechnung wie in Verfahren 51 */
#if DEBUG>0
      case 3086:
         if(retvals){
            retvals->methode="86c";
            retvals->pz_methode=3086;
         }
#endif
            pz =         9 * 8
            + (kto[3]-'0') * 7
            + (kto[4]-'0') * 6
            + (kto[5]-'0') * 5
            + (kto[6]-'0') * 4
            + (kto[7]-'0') * 3
            + (kto[8]-'0') * 2;

            MOD_11_176;   /* pz%=11 */
            if(pz<=1)
               pz=0;
            else
               pz=11-pz;
            CHECK_PZX10;

            /* Ausnahme, Variante 2 */
#if DEBUG>0
      case 4086:
         if(retvals){
            retvals->methode="86d";
            retvals->pz_methode=4086;
         }
#endif
               pz = (kto[0]-'0') * 10
               + (kto[1]-'0') * 9
               +            9 * 8
               + (kto[3]-'0') * 7
               + (kto[4]-'0') * 6
               + (kto[5]-'0') * 5
               + (kto[6]-'0') * 4
               + (kto[7]-'0') * 3
               + (kto[8]-'0') * 2;

               MOD_11_352;   /* pz%=11 */
               if(pz<=1)
                  pz=0;
               else
                  pz=11-pz;
               CHECK_PZ10;
            }

            /* Methode A */
#if DEBUG>0
      case 1086:
         if(retvals){
            retvals->methode="86a";
            retvals->pz_methode=1086;
         }
#endif
#ifdef __ALPHA
            pz =  (kto[3]-'0')
               + ((kto[4]<'5') ? (kto[4]-'0')*2 : (kto[4]-'0')*2-9)
               +  (kto[5]-'0')
               + ((kto[6]<'5') ? (kto[6]-'0')*2 : (kto[6]-'0')*2-9)
               +  (kto[7]-'0')
               + ((kto[8]<'5') ? (kto[8]-'0')*2 : (kto[8]-'0')*2-9);
#else
            pz =(kto[3]-'0')+(kto[5]-'0')+(kto[7]-'0');
            if(kto[4]<'5')pz+=(kto[4]-'0')*2; else pz+=(kto[4]-'0')*2-9;
            if(kto[6]<'5')pz+=(kto[6]-'0')*2; else pz+=(kto[6]-'0')*2-9;
            if(kto[8]<'5')pz+=(kto[8]-'0')*2; else pz+=(kto[8]-'0')*2-9;
#endif

            MOD_10_80;   /* pz%=10 */
         if(pz)pz=10-pz;
         CHECK_PZX10;

            /* Methode B */
#if DEBUG>0
      case 2086:
         if(retvals){
            retvals->methode="86b";
            retvals->pz_methode=2086;
         }
#endif
         pz = (kto[3]-'0') * 7
            + (kto[4]-'0') * 6
            + (kto[5]-'0') * 5
            + (kto[6]-'0') * 4
            + (kto[7]-'0') * 3
            + (kto[8]-'0') * 2;

         MOD_11_176;   /* pz%=11 */
         if(pz<=1)
            pz=0;
         else
            pz=11-pz;
         CHECK_PZ10;

/*  Berechnung nach der Methode 87 +§§§4 */
/*
 * ######################################################################
 * #          Berechnung nach der Methode 87 (geändert zum 6.9.04)      #
 * ######################################################################
 * # Ausnahme: (neu zum 6.9.04, der Rest ist gleich geblieben)          #
 * # Ist nach linksbündiger Auffüllung mit Nullen auf 10 Stellen die    #
 * # 3. Stelle der Kontonummer = 9 (Sachkonten), so erfolgt die         #
 * # Berechnung gemäß der Ausnahme in Methode 51 mit den                #
 * # gleichen Ergebnissen und Testkontonummern.                         #
 * #                                                                    #
 * # Methode A:                                                         #
 * # Vorgegebener Pascalcode, anzuwenden auf Stellen 5 bis 9            #
 * # von links der Kontonummer, Prüfziffer in Stelle 10.                #
 * # Der vorgegebener Pseudocode (pascal-ähnlich) wurde nach C          #
 * # umgeschrieben. Eine Beschreibung des Berechnungsverfahrens findet  #
 * # sich in der Datei pz<mmyy>.pdf (z.B. pz0602.pdf) der  Deutschen    #
 * # Bundesbank.                                                        #
 * #                                                                    #
 * # Methode B:                                                         #
 * # Modulus 11, Gewichtung 2, 3, 4, 5, 6                               #
 * # Die für die Berechnung relevanten Stellen werden von rechts        #
 * # nach links mit den Ziffern 2, 3, 4, 5, 6 multipliziert. Die        #
 * # weitere Berechnung und die möglichen Ergebnisse entsprechen dem    #
 * # Verfahren 33.                                                      #
 * # Führt die Berechnung nach Methode B wiederum zu einem              #
 * # Prüfzifferfehlen, ist eine weitere Berechnung nach Methode C       #
 * # vorzunehmen.                                                       #
 * #                                                                    #
 * # Methode C:                                                         #
 * # Modulus 7, Gewichtung 2, 3, 4, 5, 6                                #
 * # Die Stellen 5 bis 9 der Kontonummer werden von rechts nach         #
 * # links mit den Gewichten multipliziert. Die jeweiligen Produkte     #
 * # werden addiert. Die Summe ist durch 7 zu dividieren. Der           #
 * # verbleibende Rest wird vom Divisor (7) subtrahiert. Das            #
 * # Ergebnis ist die Prüfziffer. Verbleibt nach der Division kein      #
 * # Rest, ist die Prüfziffer = 0.                                      #
 * ######################################################################
 */

      case 87:

                /* Ausnahme, Variante 1 */
         if(*(kto+2)=='9'){   /* Berechnung wie in Verfahren 51 */
#if DEBUG>0
      case 4087:
         if(retvals){
            retvals->methode="87d";
            retvals->pz_methode=4087;
         }
#endif
            pz =         9 * 8
            + (kto[3]-'0') * 7
            + (kto[4]-'0') * 6
            + (kto[5]-'0') * 5
            + (kto[6]-'0') * 4
            + (kto[7]-'0') * 3
            + (kto[8]-'0') * 2;

            MOD_11_176;   /* pz%=11 */
            if(pz<=1)
               pz=0;
            else
               pz=11-pz;
            CHECK_PZX10;

            /* Ausnahme, Variante 2 */
#if DEBUG>0
      case 5087:
         if(retvals){
            retvals->methode="87e";
            retvals->pz_methode=5087;
         }
#endif
               pz = (kto[0]-'0') * 10
               + (kto[1]-'0') * 9
               +            9 * 8
               + (kto[3]-'0') * 7
               + (kto[4]-'0') * 6
               + (kto[5]-'0') * 5
               + (kto[6]-'0') * 4
               + (kto[7]-'0') * 3
               + (kto[8]-'0') * 2;

               MOD_11_352;   /* pz%=11 */
               if(pz<=1)
                  pz=0;
               else
                  pz=11-pz;
               CHECK_PZ10;
            }

#if DEBUG>0
      case 1087:
         if(retvals){
            retvals->methode="87a";
            retvals->pz_methode=1087;
         }
#endif
            /* Der Startindex für das Array konto[] ist 1, nicht wie in C
             * üblich 0; daher hat das es auch 11 Elemente (nicht wie in der
             * Beschreibung angegeben 10). Daß das so ist, sieht man an der
             * Initialisierung von i mit 4, an der Schleife [while(i<10)]
             * sowie am Ende des Verfahrens, wo p mit konto[10] verglichen
             * wird.
             * Konsequenterweise werden die beiden Arrays tab1 und tab2
             * mit dem Startindex 0 eingeführt ;-(((.
             */

         for(i=1,ptr=kto;i<11;)konto[i++]= *ptr++-'0';
         i=4;
         while(konto[i]==0)i++;
         c2=i%2;
         d2=0;
         a5=0;

         while(i<10){
            switch(konto[i]){
               case 0: konto[i]= 5; break;
               case 1: konto[i]= 6; break;
               case 5: konto[i]=10; break;
               case 6: konto[i]= 1; break;
            }

            if(c2==d2){
               if(konto[i]>5){
                  if(c2==0 && d2==0){
                     c2=d2=1;
                     a5=a5+6-(konto[i]-6);
                  }
                  else{
                     c2=d2=0;
                     a5=a5+konto[i];
                  }
               }
               else{
                  if(c2==0 && d2==0){
                     c2=1;
                     a5=a5+konto[i];
                  }
                  else{
                     c2=0;
                     a5=a5+konto[i];
                  }
               }
            }
            else{
               if(konto[i]>5){
                  if(c2==0){
                     c2=1;
                     d2=0;
                     a5=a5-6+(konto[i]-6);
                  }
                  else{
                     c2=0;
                     d2=1;
                     a5=a5-konto[i];
                  }
               }
               else{
                  if(c2==0){
                     c2=1;
                     a5=a5-konto[i];
                  }
                  else{
                     c2=0;
                     a5=a5-konto[i];
                  }
               }
            }
            i++;
         }
         while(a5<0 || a5>4){
            if(a5>4)
               a5=a5-5;
            else
               a5=a5+5;
         }
         if(d2==0)
            p=tab1[a5];
         else
            p=tab2[a5];
         if(p==konto[10])
            return OK; /* Prüfziffer ok; */
         else{
            if(konto[4]==0){
               if(p>4)
                  p=p-5;
               else
                  p=p+5;
               if(p==konto[10])return OK; /* Prüfziffer ok; */
            }
         }

            /* Methode B: Verfahren 33 */
#if DEBUG>0
      case 2087:
         if(retvals){
            retvals->methode="87b";
            retvals->pz_methode=2087;
         }
#endif
         pz = (kto[4]-'0') * 6
            + (kto[5]-'0') * 5
            + (kto[6]-'0') * 4
            + (kto[7]-'0') * 3
            + (kto[8]-'0') * 2;

         MOD_11_176;   /* pz%=11 */
         if(pz<=1)
            pz=0;
         else
            pz=11-pz;
         CHECK_PZX10;

            /* Methode C: Verfahren 84 Variante 2 */
#if DEBUG>0
      case 3087:
         if(retvals){
            retvals->methode="87c";
            retvals->pz_methode=3087;
         }
#endif
         pz = (kto[4]-'0') * 6
            + (kto[5]-'0') * 5
            + (kto[6]-'0') * 4
            + (kto[7]-'0') * 3
            + (kto[8]-'0') * 2;

         MOD_7_112;   /* pz%=7 */
         if(pz)pz=7-pz;
#if DEBUG>0
         if(retvals){
            retvals->pz=pz;
            retvals->pz_pos=10;
         }
#endif
         if(pz==*(kto+9)-'0')
            return OK;
         else
            return FALSE;


/*  Berechnung nach der Methode 88 +§§§4 */
/*
 * ######################################################################
 * #              Berechnung nach der Methode 88                        #
 * ######################################################################
 * # Modulus 11, Gewichtung 2, 3, 4, 5, 6, 7.                           #
 * # Die Stellen 4 bis 9 werden von rechts nach links mit den           #
 * # Gewichten 2, 3, 4, 5, 6, 7 multipliziert. Die weitere Berechnung   #
 * # entspricht dem Verfahren 06.                                       #
 * # Ausnahme: Ist die 3. Stelle der Kontonummer = 9, so werden         #
 * # die Stellen 3 bis 9 von rechts nach links mit den Gewichten        #
 * # 2, 3, 4, 5, 6, 7, 8 multipliziert.                                 #
 * ######################################################################
 */
      case 88:
#if DEBUG>0
         if(retvals){
            retvals->methode="88";
            retvals->pz_methode=88;
         }
#endif
         pz = (kto[3]-'0') * 7
            + (kto[4]-'0') * 6
            + (kto[5]-'0') * 5
            + (kto[6]-'0') * 4
            + (kto[7]-'0') * 3
            + (kto[8]-'0') * 2;
         if(kto[2]=='9')pz+= 9*8;   /* Ausnahme */

         MOD_11_176;   /* pz%=11 */
         if(pz<=1)
            pz=0;
         else
            pz=11-pz;
         CHECK_PZ10;

/*  Berechnung nach der Methode 89 +§§§4 */
/*
 * ######################################################################
 * #              Berechnung nach der Methode 89                        #
 * ######################################################################
 * # 8- und 9-stellige Kontonummern sind mit dem                        #
 * # Berechnungsverfahren 10 zu prüfen.                                 #
 * #                                                                    #
 * # 7-stellige Kontonummern sind wie folgt zu prüfen:                  #
 * # Modulus 11, Gewichtung 2, 3, 4, 5, 6, 7                            #
 * # Die Kontonummer ist durch linksbündige Nullenauffüllung            #
 * # 10-stellig darzustellen. Die für die Berechnung relevante          #
 * # 6-stellige Stammnummer (x) befindet sich in den Stellen 4 bis      #
 * # 9, die Prüfziffer in Stelle 10 der Kontonummer. Die einzelnen      #
 * # Stellen der Stammnummer sind von rechts nach links mit den         #
 * # Ziffern 2, 3, 4, 5, 6, 7 zu multiplizieren. Die jeweiligen         #
 * # Produkte werden addiert, nachdem jeweils aus den 2- stelligen      #
 * # Produkten Quersummen gebildet wurden. Die Summe ist durch 11       #
 * # zu dividieren. Die weiteren Berechnungen und Ergebnisse            #
 * # entsprechen dem Verfahren 06.                                      #
 * #                                                                    #
 * # 1- bis 6- und 10-stellige Kontonummern sind nicht zu               #
 * # prüfen, da diese keine Prüfziffer enthalten.                       #
 * # Testkontonummern: 1098506, 32028008, 218433000                     #
 * ######################################################################
 */
      case 89:

            /* 8- und 9-stellige Kontonummern: Verfahren 10 */
#if DEBUG>0
      case 1089:
         if(retvals){
            retvals->methode="89a";
            retvals->pz_methode=1089;
         }
#endif
         if(*kto=='0' && (*(kto+1)!='0' || *(kto+2)!='0')){
            pz = (kto[1]-'0') * 9
               + (kto[2]-'0') * 8
               + (kto[3]-'0') * 7
               + (kto[4]-'0') * 6
               + (kto[5]-'0') * 5
               + (kto[6]-'0') * 4
               + (kto[7]-'0') * 3
               + (kto[8]-'0') * 2;

            MOD_11_352;   /* pz%=11 */
            if(pz<=1)
               pz=0;
            else
               pz=11-pz;
            CHECK_PZ10;
         }

            /* 7-stellige Kontonummern */
#if DEBUG>0
      case 2089:
         if(retvals){
            retvals->methode="89b";
            retvals->pz_methode=2089;
         }
#endif
         if(*kto=='0' && *(kto+1)=='0' && *(kto+2)=='0' && *(kto+3)!='0'){
            pz=(kto[3]-'0')*7;
            if(pz>=40){
               if(pz>=50){
                  if(pz>=60)
                     pz-=54;
                  else
                     pz-=45;
               }
               else
                  pz-=36;
            }
            else if(pz>=20){
               if(pz>=30)
                  pz-=27;
               else
                  pz-=18;
            }
            else if(pz>=10)
               pz-=9;


            p1=(kto[4]-'0')*6;
            if(p1>=40){
               if(p1>=50)
                  p1-=45;
               else
                  p1-=36;
            }
            else if(p1>=20){
               if(p1>=30)
                  p1-=27;
               else
                  p1-=18;
            }
            else if(p1>=10)
               p1-=9;
            pz+=p1;

            p1=(kto[5]-'0')*5;
            if(p1>=40)
               p1-=36;
            else if(p1>=20){
               if(p1>=30)
                  p1-=27;
               else
                  p1-=18;
            }
            else if(p1>=10)
               p1-=9;
            pz+=p1;

            p1=(kto[6]-'0')*4;
            if(p1>=20){
               if(p1>=30)
                  p1-=27;
               else
                  p1-=18;
            }
            else if(p1>=10)
               p1-=9;
            pz+=p1;

            p1=(kto[7]-'0')*3;
            if(p1>=20)
               p1-=18;
            else if(p1>=10)
               p1-=9;
            pz+=p1;

            p1 = (kto[8]-'0') * 2;
            if(p1>=10)p1-=9;
            pz+=p1;

            MOD_11_44;   /* pz%=11 */
            if(pz<=1)
               pz=0;
            else
               pz=11-pz;
            CHECK_PZ10;
         }

            /* 1- bis 6- und 10-stellige Kontonummern */
#if DEBUG>0
      case 3089:
         if(retvals){
            retvals->methode="89c";
            retvals->pz_methode=3089;
         }
#endif
#if BAV_KOMPATIBEL
         return BAV_FALSE;
#else
         return OK_NO_CHK;
#endif


/* Berechnungsmethoden 90 bis 99 +§§§3
   Berechnung nach der Methode 90 +§§§4 */
/*
 * ######################################################################
 * #      Berechnung nach der Methode 90 (geändert zum 6.6.2005)        #
 * ######################################################################
 * # 1. Kundenkonten                                                    #
 * # A. Modulus 11, Gewichtung 2, 3, 4, 5, 6, 7. -> Methode 32          #
 * # B. Modulus 11, Gewichtung 2, 3, 4, 5, 6.    -> Methode 33          #
 * # C. Modulus  7, Gewichtung 2, 3, 4, 5, 6.    -> Methode 33 mod7     #
 * # D. Modulus  9, Gewichtung 2, 3, 4, 5, 6.    -> Methode 33 mod9     #
 * # E. Modulus 10, Gewichtung 2, 1, 2, 1, 2.    -> Methode 33 mod10    #
 * #                                                                    #
 * # Die Kontonummer ist immer 10-stellig. Die für die Berechnung       #
 * # relevante Kundennummer befindet sich bei der Methode A in den      #
 * # Stellen 4 bis 9 der Kontonummer und bei den Methoden B - E in      #
 * # den Stellen 5 bis 9, die Prüfziffer in Stelle 10.                  #
 * #                                                                    #
 * # Ergibt die erste Berechnung der Prüfziffer nach dem Verfahren A    #
 * # einen Prüfziffernfehler, so sind weitere Berechnungen mit den      #
 * # anderen Methoden vorzunehmen.                                      #
 * # Die Methode A enstpricht Verfahren 32. Die Methoden B - E          #
 * # entsprechen Verfahren 33, jedoch mit Divisoren 11, 7, 9 und 10.    #
 * #                                                                    #
 * # Ausnahme: Ist nach linksbündigem Auffüllen mit Nullen auf 10       #
 * # Stellen die 3. Stelle der Kontonummer = 9 (Sachkonten) befindet    #
 * # sich die für die Berechnung relevante Sachkontonummer (S) in       #
 * # den Stellen 3 bis 9. Diese Kontonummern sind ausschließlich        #
 * # nach Methode F zu prüfen.                                          #
 * #                                                                    #
 * # 2. Sachkonten -> Methode 32 (modifiziert)                          #
 * # F. Modulus 11, Gewichtung 2, 3, 4, 5, 6, 7, 8.                     #
 * # Die 3. Stelle ist 9, die für die Berechnung relevanten  Stellen    #
 * # befinden sich in den Stellen 3 bis 9.                              #
 * ######################################################################
 */

      case 90:

            /* Sachkonto */
         if(*(kto+2)=='9'){ /* geändert zum 6.6.2005; vorher waren 3. und 4. Stelle 9 */
#if DEBUG>0
      case 6090:
         if(retvals){
            retvals->methode="90f";
            retvals->pz_methode=6090;
         }
#endif
         pz =      9       * 8 /* immer 9; kann vom Compiler optimiert werden */
            + (kto[3]-'0') * 7
            + (kto[4]-'0') * 6
            + (kto[5]-'0') * 5
            + (kto[6]-'0') * 4
            + (kto[7]-'0') * 3
            + (kto[8]-'0') * 2;

         MOD_11_176;   /* pz%=11 */
            if(pz<=1)
               pz=0;
            else
               pz=11-pz;
            CHECK_PZ10;
         }

            /* Methode A */
#if DEBUG>0
      case 1090:
         if(retvals){
            retvals->methode="90a";
            retvals->pz_methode=1090;
         }
#endif
         pz = (kto[3]-'0') * 7
            + (kto[4]-'0') * 6
            + (kto[5]-'0') * 5
            + (kto[6]-'0') * 4
            + (kto[7]-'0') * 3
            + (kto[8]-'0') * 2;

         MOD_11_176;   /* pz%=11 */
         if(pz<=1)
            pz=0;
         else
            pz=11-pz;
         CHECK_PZX10;

            /* Methode B */
#if DEBUG>0
      case 2090:
         if(retvals){
            retvals->methode="90b";
            retvals->pz_methode=2090;
         }
#endif
         pz = (kto[4]-'0') * 6
            + (kto[5]-'0') * 5
            + (kto[6]-'0') * 4
            + (kto[7]-'0') * 3
            + (kto[8]-'0') * 2;

         MOD_11_176;   /* pz%=11 */
         if(pz<=1)
            pz=0;
         else
            pz=11-pz;
         CHECK_PZX10;

            /* Methode C */
#if DEBUG>0
      case 3090:
         if(retvals){
            retvals->methode="90c";
            retvals->pz_methode=3090;
         }
#endif
         pz = (kto[4]-'0') * 6
            + (kto[5]-'0') * 5
            + (kto[6]-'0') * 4
            + (kto[7]-'0') * 3
            + (kto[8]-'0') * 2;

         MOD_7_112;   /* pz%=7 */
         if(pz)pz=7-pz;
         CHECK_PZX10;

            /* Methode D */
#if DEBUG>0
      case 4090:
         if(retvals){
            retvals->methode="90d";
            retvals->pz_methode=4090;
         }
#endif
         pz = (kto[4]-'0') * 6
            + (kto[5]-'0') * 5
            + (kto[6]-'0') * 4
            + (kto[7]-'0') * 3
            + (kto[8]-'0') * 2;

         MOD_9_144;   /* pz%=9 */
         if(pz)pz=9-pz;
         CHECK_PZX10;

            /* Methode E */
#if DEBUG>0
      case 5090:
         if(retvals){
            retvals->methode="90e";
            retvals->pz_methode=5090;
         }
#endif
         pz = (kto[4]-'0') * 2
            + (kto[5]-'0')
            + (kto[6]-'0') * 2
            + (kto[7]-'0')
            + (kto[8]-'0') * 2;

         MOD_10_40;   /* pz%=10 */
         if(pz)pz=10-pz;
         CHECK_PZ10;

/*  Berechnung nach der Methode 91 +§§§4 */
/*
 * ######################################################################
 * #   Berechnung nach der Methode 91 (geändert zum 8.12.03)            #
 * ######################################################################
 * # 1. Modulus 11, Gewichtung 2, 3, 4, 5, 6, 7                         #
 * # 2. Modulus 11, Gewichtung 7, 6, 5, 4, 3, 2                         #
 * # 3. Modulus 11, Gewichtung 2, 3, 4, 0, 5, 6, 7, 8, 9, A (A = 10)    #
 * # 4. Modulus 11, Gewichtung 2, 4, 8, 5, 10, 9                        #
 * #                                                                    #
 * # Gemeinsame Hinweise für die Berechnungsvarianten 1 bis 4:          #
 * #                                                                    #
 * # Die Kontonummer ist immer 10-stellig. Die einzelnen Stellen        #
 * # der Kontonummer werden von links nach rechts von 1 bis 10          #
 * # durchnummeriert. Die Stelle 7 der Kontonummer ist die              #
 * # Prüfziffer. Die für die Berechnung relevanten Kundennummern        #
 * # (K) sind von rechts nach links mit den jeweiligen Gewichten zu     #
 * # multiplizieren. Die restliche Berechnung und möglichen             #
 * # Ergebnisse entsprechen dem Verfahren 06.                           #
 * #                                                                    #
 * # Ergibt die Berechnung nach der ersten beschriebenen Variante       #
 * # einen Prüfzifferfehler, so sind in der angegebenen Reihenfolge     #
 * # weitere Berechnungen mit den anderen Varianten                     #
 * # vorzunehmen, bis die Berechnung keinen Prüfzifferfehler mehr       #
 * # ergibt. Kontonummern, die endgültig nicht zu einem richtigen       #
 * # Ergebnis führen, sind nicht prüfbar.                               #
 * #                                                                    #
 * # Variante 1:                                                        #
 * # Modulus 11, Gewichtung 2, 3, 4, 5, 6, 7                            #
 * # Die Stellen 8 bis 10 werden nicht in die Berechnung                #
 * # einbezogen.                                                        #
 * #                                                                    #
 * # Variante 2:                                                        #
 * # Modulus 11, Gewichtung 7, 6, 5, 4, 3, 2                            #
 * # Die Stellen 8 bis 10 werden nicht in die Berechnung                #
 * # einbezogen.                                                        #
 * #                                                                    #
 * # Variante 3:                                                        #
 * # Modulus 11, Gewichtung 2, 3, 4, 0, 5, 6, 7, 8, 9, A (A = 10)       #
 * # Die Stellen 1 bis 10 werden in die Berechnung einbezogen.          #
 * #                                                                    #
 * # Variante 4:                                                        #
 * # Modulus 11, Gewichtung 2, 4, 8, 5, A, 9 (A = 10)                   #
 * # Die Stellen 8 bis 10 werden nicht in die Berechnung einbezogen.    #
 * ######################################################################
 */
      case 91:

            /* Methode A */
#if DEBUG>0
      case 1091:
         if(retvals){
            retvals->methode="91a";
            retvals->pz_methode=1091;
         }
#endif
         pz = (kto[0]-'0') * 7
            + (kto[1]-'0') * 6
            + (kto[2]-'0') * 5
            + (kto[3]-'0') * 4
            + (kto[4]-'0') * 3
            + (kto[5]-'0') * 2;

         MOD_11_176;   /* pz%=11 */
         if(pz<=1)
            pz=0;
         else
            pz=11-pz;
         CHECK_PZX7;

            /* Methode B */
#if DEBUG>0
      case 2091:
         if(retvals){
            retvals->methode="91b";
            retvals->pz_methode=2091;
         }
#endif
         pz = (kto[0]-'0') * 2
            + (kto[1]-'0') * 3
            + (kto[2]-'0') * 4
            + (kto[3]-'0') * 5
            + (kto[4]-'0') * 6
            + (kto[5]-'0') * 7;

         MOD_11_176;   /* pz%=11 */
         if(pz<=1)
            pz=0;
         else
            pz=11-pz;
         CHECK_PZX7;

            /* Methode C */
#if DEBUG>0
      case 3091:
         if(retvals){
            retvals->methode="91c";
            retvals->pz_methode=3091;
         }
#endif
         pz = (kto[0]-'0') * 10
            + (kto[1]-'0') * 9
            + (kto[2]-'0') * 8
            + (kto[3]-'0') * 7
            + (kto[4]-'0') * 6
            + (kto[5]-'0') * 5
            + (kto[7]-'0') * 4
            + (kto[8]-'0') * 3
            + (kto[9]-'0') * 2;

         MOD_11_352;   /* pz%=11 */
         if(pz<=1)
            pz=0;
         else
            pz=11-pz;
         CHECK_PZX7;

            /* Methode D */
#if DEBUG>0
      case 4091:
         if(retvals){
            retvals->methode="91d";
            retvals->pz_methode=4091;
         }
#endif
         pz = (kto[0]-'0') * 9
            + (kto[1]-'0') * 10
            + (kto[2]-'0') * 5
            + (kto[3]-'0') * 8
            + (kto[4]-'0') * 4
            + (kto[5]-'0') * 2;

         MOD_11_176;   /* pz%=11 */
         if(pz<=1)
            pz=0;
         else
            pz=11-pz;
         CHECK_PZ7;

/*  Berechnung nach der Methode 92 +§§§4 */
/*
 * ######################################################################
 * #              Berechnung nach der Methode 92                        #
 * ######################################################################
 * # Modulus 10, Gewichtung 3, 7, 1, 3, 7, 1.                           #
 * # Die Berechnung erfolgt wie bei Verfahren 01, jedoch werden nur     #
 * # die Stellen 4 bis 9 einbezogen. Stelle 10 ist die Prüfziffer.      #
 * ######################################################################
 */
      case 92:
#if DEBUG>0
         if(retvals){
            retvals->methode="92";
            retvals->pz_methode=92;
         }
#endif
         pz = (kto[3]-'0')
            + (kto[4]-'0') * 7
            + (kto[5]-'0') * 3
            + (kto[6]-'0')
            + (kto[7]-'0') * 7
            + (kto[8]-'0') * 3;

         MOD_10_160;   /* pz%=10 */
         if(pz)pz=10-pz;
         CHECK_PZ10;

/*  Berechnung nach der Methode 93 +§§§4 */
/*
 * ######################################################################
 * #              Berechnung nach der Methode 93                        #
 * ######################################################################
 * # Modulus 11, Gewichtung 2, 3, 4, 5, 6.                              #
 * # Die für die Berechnung relevante Kundennummer befindet sich        #
 * # entweder                                                           #
 * # a) in den Stellen 1 bis 5, die Prüfziffer in Stelle  6,            #
 * # b) in den Stellen 5 bis 9, die Prüfziffer in Stelle 10.            #
 * # Die 2-stellige Unternummer und die 2-stellige Kontoartnummer       #
 * # werden nicht in die Berechnung einbezogen. Sie befinden sich im    #
 * # Fall a) an Stelle 7 bis 10. Im Fall b) befinden Sie sich an        #
 * # Stelle 1 bis 4 und müssen "0000" lauten.                           #
 * # Die 5-stellige Kundennummer wird von rechts nach links mit den     #
 * # Gewichten multipliziert. Die weitere Berechnung und die            #
 * # möglichen Ergebnisse entsprechen dem Verfahren 06.                 #
 * # Führt die Berechnung zu einem Prüfziffernfehler, so ist die        #
 * # Berechnung nach Variante 2 vorzunehmen. Das Berechnungsverfahren   #
 * # entspricht Variante 1. Die Summe der Produkte ist jedoch durch     #
 * # 7 zu dividieren. Der verbleibende Rest wird vom Divisor (7)        #
 * # subtrahiert. Das Ergebnis ist die Prüfziffer.                      #
 * ######################################################################
 */
      case 93:

            /* Variante 1 */
         if(*kto=='0' && *(kto+1)=='0' && *(kto+2)=='0' && *(kto+3)=='0'){   /* Fall b) */
#if DEBUG>0
      case 2093:
         if(retvals){
            retvals->methode="93b";
            retvals->pz_methode=2093;
         }
#endif
            pz = (kto[4]-'0') * 6
               + (kto[5]-'0') * 5
               + (kto[6]-'0') * 4
               + (kto[7]-'0') * 3
               + (kto[8]-'0') * 2;

            p1=pz;
            MOD_11_176;   /* pz%=11 */
         }
         else{
#if DEBUG>0
      case 1093:
         if(retvals){
            retvals->methode="93a";
            retvals->pz_methode=1093;
         }
#endif
            pz = (kto[0]-'0') * 6
               + (kto[1]-'0') * 5
               + (kto[2]-'0') * 4
               + (kto[3]-'0') * 3
               + (kto[4]-'0') * 2;

            kto[9]=kto[5];  /* Prüfziffer nach Stelle 10 */
            p1=pz;
            MOD_11_176;   /* pz%=11 */
         }
         if(pz<=1)
            pz=0;
         else
            pz=11-pz;
         CHECK_PZX10;

            /* Variante 2 */
#if DEBUG>0
            /* nicht optimiert, da dieser Teil nur in der DEBUG-Version benutzt wird */

         if(untermethode){ /* pz wurde noch nicht berechnet */
            if(*kto=='0' && *(kto+1)=='0' && *(kto+2)=='0' && *(kto+3)=='0'){   /* Fall b) */
#if DEBUG>0
      case 4093:
         if(retvals){
            retvals->methode="93d";
            retvals->pz_methode=4093;
         }
#endif
               for(p1=0,ptr=kto+8,i=0;i<5;ptr--,i++)
                  p1+=(*ptr-'0')*w93[i];
            }
            else{
#if DEBUG>0
      case 3093:
         if(retvals){
            retvals->methode="93c";
            retvals->pz_methode=3093;
         }
#endif
               for(p1=0,ptr=kto+4,i=0;i<5;ptr--,i++)
                  p1+=(*ptr-'0')*w93[i];
                  *(kto+9)= *(kto+5);  /* Prüfziffer nach Stelle 10 */
            }
         }
#endif
         pz=p1%7;
         if(pz)pz=7-pz;
         CHECK_PZ10;

/*  Berechnung nach der Methode 94 +§§§4 */
/*
 * ######################################################################
 * #              Berechnung nach der Methode 94                        #
 * ######################################################################
 * # Modulus 10, Gewichtung 1, 2, 1, 2, 1, 2, 1, 2, 1.                  #
 * # Die Stellen 1 bis 9 der Kontonummer sind von rechts nach links     #
 * # mit den Gewichten zu multiplizieren. Die weitere Berechnung        #
 * # erfolgt wie bei Verfahren 00.                                      #
 * ######################################################################
 */
      case 94:
#if DEBUG>0
         if(retvals){
            retvals->methode="94";
            retvals->pz_methode=94;
         }
#endif
#ifdef __ALPHA
         pz =  (kto[0]-'0')
            + ((kto[1]<'5') ? (kto[1]-'0')*2 : (kto[1]-'0')*2-9)
            +  (kto[2]-'0')
            + ((kto[3]<'5') ? (kto[3]-'0')*2 : (kto[3]-'0')*2-9)
            +  (kto[4]-'0')
            + ((kto[5]<'5') ? (kto[5]-'0')*2 : (kto[5]-'0')*2-9)
            +  (kto[6]-'0')
            + ((kto[7]<'5') ? (kto[7]-'0')*2 : (kto[7]-'0')*2-9)
            +  (kto[8]-'0');
#else
         pz =(kto[0]-'0')+(kto[2]-'0')+(kto[4]-'0')+(kto[6]-'0')+(kto[8]-'0');
         if(kto[1]<'5')pz+=(kto[1]-'0')*2; else pz+=(kto[1]-'0')*2-9;
         if(kto[3]<'5')pz+=(kto[3]-'0')*2; else pz+=(kto[3]-'0')*2-9;
         if(kto[5]<'5')pz+=(kto[5]-'0')*2; else pz+=(kto[5]-'0')*2-9;
         if(kto[7]<'5')pz+=(kto[7]-'0')*2; else pz+=(kto[7]-'0')*2-9;
#endif

         MOD_10_80;   /* pz%=10 */
         if(pz)pz=10-pz;
         CHECK_PZ10;

/*  Berechnung nach der Methode 95 +§§§4 */
/*
 * ######################################################################
 * #              Berechnung nach der Methode 95                        #
 * ######################################################################
 * # Modulus 11, Gewichtung 2, 3, 4, 5, 6, 7, 2, 3, 4                   #
 * # Die Berechnung erfolgt wie bei Verfahren 06.                       #
 * # Ausnahmen:                                                         #
 * # Kontonr.: 0000000001 bis 0001999999                                #
 * # Kontonr.: 0009000000 bis 0025999999                                #
 * # Kontonr.: 0396000000 bis 0499999999                                #
 * # Kontonr.: 0700000000 bis 0799999999                                #
 * # Für diese Kontonummernkreise ist keine Prüfzifferberechnung        #
 * # möglich. Sie sind als richtig anzusehen.                           #
 * ######################################################################
 */
      case 95:
#if DEBUG>0
         if(retvals){
            retvals->methode="95";
            retvals->pz_methode=95;
         }
#endif
        if(   /* Ausnahmen: keine Prüfzifferberechnung */
            (strcmp(kto,"0000000001")>=0 && strcmp(kto,"0001999999")<=0)
         || (strcmp(kto,"0009000000")>=0 && strcmp(kto,"0025999999")<=0)
         || (strcmp(kto,"0396000000")>=0 && strcmp(kto,"0499999999")<=0)
         || (strcmp(kto,"0700000000")>=0 && strcmp(kto,"0799999999")<=0))
            return OK_NO_CHK;
         pz = (kto[0]-'0') * 4
            + (kto[1]-'0') * 3
            + (kto[2]-'0') * 2
            + (kto[3]-'0') * 7
            + (kto[4]-'0') * 6
            + (kto[5]-'0') * 5
            + (kto[6]-'0') * 4
            + (kto[7]-'0') * 3
            + (kto[8]-'0') * 2;

         MOD_11_176;   /* pz%=11 */
         if(pz<=1)
            pz=0;
         else
            pz=11-pz;
         CHECK_PZ10;

/*  Berechnung nach der Methode 96 +§§§4 */
/*
 * ######################################################################
 * #              Berechnung nach der Methode 96                        #
 * ######################################################################
 * # A. Modulus 11, Gewichtung 2,3,4,5,6,7,8,9,1                        #
 * # B. Modulus 10, Gewichtung 2,1,2,1,2,1,2,1,2                        #
 * # Die Prüfziffernberechnung ist nach Kennziffer 19 durchzuführen.    #
 * # Führt die Berechnung zu einem Fehler, so ist sie nach Kennziffer   #
 * # 00 durchzuführen. Führen beide Varianten zu einem Fehler, so       #
 * # gelten Kontonummern zwischen 0001300000 und 0099399999 als         #
 * # richtig.                                                           #
 * ######################################################################
 */
      case 96:

#if DEBUG>0
      case 3096:
         if(retvals){
            retvals->methode="96c";
            retvals->pz_methode=3096;
         }
#endif
            /* die Berechnung muß in diesem Fall nicht gemacht werden */
         if(strcmp(kto,"0001300000")>=0 && strcmp(kto,"0099400000")<0)
            return OK_NO_CHK;

            /* Methode A */
#if DEBUG>0
      case 1096:
         if(retvals){
            retvals->methode="96a";
            retvals->pz_methode=1096;
         }
#endif
         pz = (kto[0]-'0')
            + (kto[1]-'0') * 9
            + (kto[2]-'0') * 8
            + (kto[3]-'0') * 7
            + (kto[4]-'0') * 6
            + (kto[5]-'0') * 5
            + (kto[6]-'0') * 4
            + (kto[7]-'0') * 3
            + (kto[8]-'0') * 2;

         MOD_11_352;   /* pz%=11 */
         if(pz<=1)
            pz=0;
         else
            pz=11-pz;
         CHECK_PZX10;

            /* Methode B */
#if DEBUG>0
      case 2096:
         if(retvals){
            retvals->methode="96b";
            retvals->pz_methode=2096;
         }
#endif
#ifdef __ALPHA
         pz = ((kto[0]<'5') ? (kto[0]-'0')*2 : (kto[0]-'0')*2-9)
            +  (kto[1]-'0')
            + ((kto[2]<'5') ? (kto[2]-'0')*2 : (kto[2]-'0')*2-9)
            +  (kto[3]-'0')
            + ((kto[4]<'5') ? (kto[4]-'0')*2 : (kto[4]-'0')*2-9)
            +  (kto[5]-'0')
            + ((kto[6]<'5') ? (kto[6]-'0')*2 : (kto[6]-'0')*2-9)
            +  (kto[7]-'0')
            + ((kto[8]<'5') ? (kto[8]-'0')*2 : (kto[8]-'0')*2-9);
#else
         pz =(kto[1]-'0')+(kto[3]-'0')+(kto[5]-'0')+(kto[7]-'0');
         if(kto[0]<'5')pz+=(kto[0]-'0')*2; else pz+=(kto[0]-'0')*2-9;
         if(kto[2]<'5')pz+=(kto[2]-'0')*2; else pz+=(kto[2]-'0')*2-9;
         if(kto[4]<'5')pz+=(kto[4]-'0')*2; else pz+=(kto[4]-'0')*2-9;
         if(kto[6]<'5')pz+=(kto[6]-'0')*2; else pz+=(kto[6]-'0')*2-9;
         if(kto[8]<'5')pz+=(kto[8]-'0')*2; else pz+=(kto[8]-'0')*2-9;
#endif

         MOD_10_80;   /* pz%=11 */
         if(pz)pz=10-pz;
         CHECK_PZ10;

/*  Berechnung nach der Methode 97 +§§§4 */
/*
 * ######################################################################
 * #              Berechnung nach der Methode 97                        #
 * ######################################################################
 * #  Modulus 11:                                                       #
 * #  Die Kontonummer (5, 6, 7, 8, 9 o. 10-stellig) ist durch links-    #
 * #  bündige Nullenauffüllung 10-stellig darzustellen. Danach ist die  #
 * #  10. Stelle die Prüfziffer.                                        #
 * #                                                                    #
 * #  Die Kontonummer ist unter Weglassung der Prüfziffer (= Wert X)    #
 * #  durch 11 zu teilen. Das Ergebnis der Division ist ohne die        #
 * #  Nachkomma-Stellen mit 11 zu multiplizieren. Das Produkt ist vom   #
 * #  'Wert X' zu subtrahieren.                                         #
 * #                                                                    #
 * #  Ist das Ergebnis < 10, so entspricht das Ergebnis der Prüfziffer. #
 * #  Ist das Ergebnis = 10, so ist die Prüfziffer = 0                  #
 * ######################################################################
 */

      case 97:
#if DEBUG>0
         if(retvals){
            retvals->methode="97";
            retvals->pz_methode=97;
         }
#endif
         if(kto[0]=='0' && kto[1]=='0' && kto[2]=='0' && kto[3]=='0' && kto[4]=='0'
               && kto[5]=='0')return INVALID_KTO;

         p1= *(kto+9);
         *(kto+9)=0;    /* Prüfziffer (temporär) löschen */
         pz=atoi(kto)%11;
         if(pz==10)pz=0;
         *(kto+9)=p1;   /* Prüfziffer wiederherstellen */
         CHECK_PZ10;

/*  Berechnung nach der Methode 98 +§§§4 */
/*
 * ######################################################################
 * #              Berechnung nach der Methode 98                        #
 * ######################################################################
 * # Modulus 10, Gewichtung 3, 1, 7, 3, 1, 7, 3                         #
 * # Die Kontonummer ist 10-stellig. Die Berechnung erfolgt wie         #
 * # bei Verfahren 01. Es ist jedoch zu beachten, dass nur die          #
 * # Stellen 3 bis 9 in die Prüfzifferberechnung einbezogen             #
 * # werden. Die Stelle 10 der Kontonummer ist die Prüfziffer.          #
 * # Führt die Berechnung zu einem falschen Ergebnis, so ist            #
 * # alternativ das Verfahren 32 anzuwenden.                            #
 * ######################################################################
 */
      case 98:
#if DEBUG>0
      case 1098:
         if(retvals){
            retvals->methode="98a";
            retvals->pz_methode=1098;
         }
#endif
         pz = (kto[2]-'0') * 3
            + (kto[3]-'0') * 7
            + (kto[4]-'0')
            + (kto[5]-'0') * 3
            + (kto[6]-'0') * 7
            + (kto[7]-'0')
            + (kto[8]-'0') * 3;

         MOD_10_160;   /* pz%=10 */
         if(pz)pz=10-pz;
         CHECK_PZX10;

            /* alternativ: Verfahren 32 */
#if DEBUG>0
      case 2098:
         if(retvals){
            retvals->methode="98b";
            retvals->pz_methode=2098;
         }
#endif
         pz = (kto[3]-'0') * 7
            + (kto[4]-'0') * 6
            + (kto[5]-'0') * 5
            + (kto[6]-'0') * 4
            + (kto[7]-'0') * 3
            + (kto[8]-'0') * 2;

         MOD_11_176;   /* pz%=11 */
         if(pz<=1)
            pz=0;
         else
            pz=11-pz;
         CHECK_PZ10;

/*  Berechnung nach der Methode 99 +§§§4 */
/*
 * ######################################################################
 * #              Berechnung nach der Methode 99                        #
 * ######################################################################
 * # Modulus 11, Gewichtung 2, 3, 4, 5, 6, 7, 2, 3, 4                   #
 * # Die Berechnung erfolgt wie bei Verfahren 06.                       #
 * # Ausnahmen: Kontonr.: 0396000000 bis 0499999999                     #
 * # Für diese Kontonummern ist keine Prüfzifferberechnung              #
 * # möglich.  Sie sind als richtig anzusehen.                          #
 * ######################################################################
 */
      case 99:
#if DEBUG>0
         if(retvals){
            retvals->methode="99";
            retvals->pz_methode=99;
         }
#endif
         if(strcmp(kto,"0396000000")>=0 && strcmp(kto,"0500000000")<0){
            pz= *(kto+9)-'0';
            return OK;
         }
         pz = (kto[0]-'0') * 4
            + (kto[1]-'0') * 3
            + (kto[2]-'0') * 2
            + (kto[3]-'0') * 7
            + (kto[4]-'0') * 6
            + (kto[5]-'0') * 5
            + (kto[6]-'0') * 4
            + (kto[7]-'0') * 3
            + (kto[8]-'0') * 2;

         MOD_11_176;   /* pz%=11 */
         if(pz<=1)
            pz=0;
         else
            pz=11-pz;
         CHECK_PZ10;

/* Berechnungsmethoden A0 bis A9 +§§§3
   Berechnung nach der Methode A0 +§§§4 */
/*
 * ######################################################################
 * #              Berechnung nach der Methode A0                       #
 * ######################################################################
 * #  Modulus 11, Gewichtung 2, 4, 8, 5, 10, 0, 0, 0, 0 Die             #
 * #  Kontonummer ist einschließlich der Prüfziffer 10- stellig,        #
 * #  ggf. ist die Kontonummer für die Prüfzifferberechnung durch       #
 * #  linksbündige Auffüllung mit Nullen 10-stellig darzustellen.       #
 * #  Die Stelle 10 ist die Prüfziffer. Die einzelnen Stellen der       #
 * #  Kontonummer (ohne Prüfziffer) sind von rechts nach links mit      #
 * #  dem zugehörigen Gewicht (2, 4, 8, 5, 10, 0, 0, 0, 0) zu           #
 * #  multiplizieren. Die Produkte werden addiert. Das Ergebnis ist     #
 * #  durch 11 zu dividieren. Ergibt sich nach der Division ein         #
 * #  Rest von 0 oder 1, so ist die Prüfziffer 0. Ansonsten ist der     #
 * #  Rest vom Divisor (11) zu subtrahieren. Das Ergebnis ist die       #
 * #  Prüfziffer.                                                       #
 * #  Ausnahme: 3-stellige Kontonummern bzw. Kontonummern, deren        #
 * #  Stellen 1 bis 7 = 0 sind, enthalten keine Prüfziffer und sind     #
 * #  als richtig anzusehen.                                            #
 * ######################################################################
 */
      case 100:
#if DEBUG>0
         if(retvals){
            retvals->methode="A0";
            retvals->pz_methode=100;
         }
#endif
         if(!strncmp(kto,"0000000",7))return OK_NO_CHK;
         pz = (kto[4]-'0') * 10
            + (kto[5]-'0') * 5
            + (kto[6]-'0') * 8
            + (kto[7]-'0') * 4
            + (kto[8]-'0') * 2;

         MOD_11_176;   /* pz%=11 */
         if(pz<=1)
            pz=0;
         else
            pz=11-pz;
         CHECK_PZ10;

/*  Berechnung nach der Methode A1 +§§§4 */
/*
 * ######################################################################
 * #    Berechnung nach der Methode A1 (geändert zum 9.6.2003)          #
 * ######################################################################
 * # Modulus 10, Gewichtung 2, 1, 2, 1, 2, 1, 2, 0, 0.                  #
 * #                                                                    #
 * # Die Kontonummern sind 8- oder 10-stellig. Kontonummern (ohne       #
 * # führende Nullen  gezählt) mit 9 oder weniger als 8 Stellen sind    #
 * # falsch. 8-stellige Kontonummern sind für die Prüfzifferberechnung  #
 * # durch linksbündige Auffüllung mit Nullen 10-stellig darzustellen.  #
 * # Die Berechnung erfolgt wie beim Verfahren 00.                      #
 * ######################################################################
 */

      case 101:
#if DEBUG>0
         if(retvals){
            retvals->methode="A1";
            retvals->pz_methode=101;
         }
#endif
         if((*kto=='0' && *(kto+1)!='0')
               || (*kto=='0' && *(kto+1)=='0' && *(kto+2)=='0'))
            return INVALID_KTO;
#ifdef __ALPHA
         pz = ((kto[2]<'5') ? (kto[2]-'0')*2 : (kto[2]-'0')*2-9)
            +  (kto[3]-'0')
            + ((kto[4]<'5') ? (kto[4]-'0')*2 : (kto[4]-'0')*2-9)
            +  (kto[5]-'0')
            + ((kto[6]<'5') ? (kto[6]-'0')*2 : (kto[6]-'0')*2-9)
            +  (kto[7]-'0')
            + ((kto[8]<'5') ? (kto[8]-'0')*2 : (kto[8]-'0')*2-9);
#else
         pz =(kto[3]-'0')+(kto[5]-'0')+(kto[7]-'0');
         if(kto[2]<'5')pz+=(kto[2]-'0')*2; else pz+=(kto[2]-'0')*2-9;
         if(kto[4]<'5')pz+=(kto[4]-'0')*2; else pz+=(kto[4]-'0')*2-9;
         if(kto[6]<'5')pz+=(kto[6]-'0')*2; else pz+=(kto[6]-'0')*2-9;
         if(kto[8]<'5')pz+=(kto[8]-'0')*2; else pz+=(kto[8]-'0')*2-9;
#endif

         MOD_10_80;   /* pz%=10 */
         if(pz)pz=10-pz;
         CHECK_PZ10;

/*  Berechnung nach der Methode A2 +§§§4 */
/*
 * ######################################################################
 * #              Berechnung nach der Methode A2                       #
 * ######################################################################
 * # Modulus 10, Gewichtung 2, 1, 2, 1, 2, 1, 2, 1, 2                   #
 * # Modulus 11, Gewichtung 2, 3, 4, 5, 6, 7, 2, 3, 4                   #
 * #                                                                    #
 * # Die Kontonummer ist einschließlich der Prüfziffer 10-stellig,      #
 * # ggf. ist die Kontonummer für die Prüfzifferberechnung durch        #
 * # linksbündige Auffüllung mit Nullen 10-stellig darzustellen.        #
 * #                                                                    #
 * # Variante 1: Gewichtung und Berechnung erfolgen nach der Methode    #
 * # 00. Führt die Berechnung nach Variante 1 zu einem                  #
 * # Prüfzifferfehler, so ist nach Variante 2 zu prüfen.                #
 * #                                                                    #
 * # Testkontonummern (richtig): 3456789019, 5678901231, 6789012348     #
 * # Testkontonummer (falsch): 3456789012, 1234567890                   #
 * #                                                                    #
 * # Variante 2: Gewichtung und Berechnung erfolgen nach der Methode    #
 * # 04.                                                                #
 * # Testkontonummer (richtig): 3456789012                              #
 * # Testkontonummern (falsch): 1234567890, 0123456789                  #
 * ######################################################################
 */
      case 102:
#if DEBUG>0
      case 1102:
         if(retvals){
            retvals->methode="A2a";
            retvals->pz_methode=1102;
         }
#endif

            /* Variante 1: Berechnung nach Methode 00 */
#ifdef __ALPHA
         pz = ((kto[0]<'5') ? (kto[0]-'0')*2 : (kto[0]-'0')*2-9)
            +  (kto[1]-'0')
            + ((kto[2]<'5') ? (kto[2]-'0')*2 : (kto[2]-'0')*2-9)
            +  (kto[3]-'0')
            + ((kto[4]<'5') ? (kto[4]-'0')*2 : (kto[4]-'0')*2-9)
            +  (kto[5]-'0')
            + ((kto[6]<'5') ? (kto[6]-'0')*2 : (kto[6]-'0')*2-9)
            +  (kto[7]-'0')
            + ((kto[8]<'5') ? (kto[8]-'0')*2 : (kto[8]-'0')*2-9);
#else
         pz =(kto[1]-'0')+(kto[3]-'0')+(kto[5]-'0')+(kto[7]-'0');
         if(kto[0]<'5')pz+=(kto[0]-'0')*2; else pz+=(kto[0]-'0')*2-9;
         if(kto[2]<'5')pz+=(kto[2]-'0')*2; else pz+=(kto[2]-'0')*2-9;
         if(kto[4]<'5')pz+=(kto[4]-'0')*2; else pz+=(kto[4]-'0')*2-9;
         if(kto[6]<'5')pz+=(kto[6]-'0')*2; else pz+=(kto[6]-'0')*2-9;
         if(kto[8]<'5')pz+=(kto[8]-'0')*2; else pz+=(kto[8]-'0')*2-9;
#endif

         MOD_10_80;   /* pz%=10 */
         if(pz)pz=10-pz;
         CHECK_PZX10;

            /* Variante 2: Berechnung nach Methode 04 */
#if DEBUG>0
      case 2102:
         if(retvals){
            retvals->methode="A2b";
            retvals->pz_methode=2102;
         }
#endif
         pz = (kto[0]-'0') * 4
            + (kto[1]-'0') * 3
            + (kto[2]-'0') * 2
            + (kto[3]-'0') * 7
            + (kto[4]-'0') * 6
            + (kto[5]-'0') * 5
            + (kto[6]-'0') * 4
            + (kto[7]-'0') * 3
            + (kto[8]-'0') * 2;

         MOD_11_176;   /* pz%=11 */
         if(pz)pz=11-pz;
         INVALID_PZ10;
         CHECK_PZ10;

/*  Berechnung nach der Methode A3 +§§§4 */
/*
 * ######################################################################
 * #              Berechnung nach der Methode A3                       #
 * ######################################################################
 * # Modulus 10, Gewichtung 2, 1, 2, 1, 2, 1, 2, 1, 2                   #
 * # Modulus 11, Gewichtung 2, 3, 4, 5, 6, 7, 8, 9, 10                  #
 * #                                                                    #
 * # Die Kontonummer ist einschließlich der Prüfziffer 10-stellig,      #
 * # ggf. ist die Kontonummer für die Prüfzifferberechnung durch        #
 * # linksbündige Auffüllung mit Nullen 10-stellig darzustellen.        #
 * #                                                                    #
 * # Variante 1: Gewichtung und Berechnung erfolgen nach der Methode    #
 * # 00. Führt die Berechnung nach Variante 1 zu einem                  #
 * # Prüfzifferfehler, so ist nach Variante 2 zu prüfen.                #
 * #                                                                    #
 * # Testkontonummern (richtig): 1234567897, 0123456782                 #
 * # Testkontonummern (falsch): 9876543210, 1234567890,                 #
 * #                            6543217890, 0543216789                  #
 * #                                                                    #
 * # Variante 2: Gewichtung und Berechnung erfolgen nach der Methode    #
 * # 10.                                                                #
 * #                                                                    #
 * # Testkontonummern (richtig): 9876543210, 1234567890, 0123456789     #
 * # Testkontonummern (falsch): 6543217890, 0543216789                  #
 * ######################################################################
 */
      case 103:

            /* Variante 1: Berechnung nach Methode 00 */
#if DEBUG>0
      case 1103:
         if(retvals){
            retvals->methode="A3a";
            retvals->pz_methode=1103;
         }
#endif
#ifdef __ALPHA
         pz = ((kto[0]<'5') ? (kto[0]-'0')*2 : (kto[0]-'0')*2-9)
            +  (kto[1]-'0')
            + ((kto[2]<'5') ? (kto[2]-'0')*2 : (kto[2]-'0')*2-9)
            +  (kto[3]-'0')
            + ((kto[4]<'5') ? (kto[4]-'0')*2 : (kto[4]-'0')*2-9)
            +  (kto[5]-'0')
            + ((kto[6]<'5') ? (kto[6]-'0')*2 : (kto[6]-'0')*2-9)
            +  (kto[7]-'0')
            + ((kto[8]<'5') ? (kto[8]-'0')*2 : (kto[8]-'0')*2-9);
#else
         pz =(kto[1]-'0')+(kto[3]-'0')+(kto[5]-'0')+(kto[7]-'0');
         if(kto[0]<'5')pz+=(kto[0]-'0')*2; else pz+=(kto[0]-'0')*2-9;
         if(kto[2]<'5')pz+=(kto[2]-'0')*2; else pz+=(kto[2]-'0')*2-9;
         if(kto[4]<'5')pz+=(kto[4]-'0')*2; else pz+=(kto[4]-'0')*2-9;
         if(kto[6]<'5')pz+=(kto[6]-'0')*2; else pz+=(kto[6]-'0')*2-9;
         if(kto[8]<'5')pz+=(kto[8]-'0')*2; else pz+=(kto[8]-'0')*2-9;
#endif

         MOD_10_80;   /* pz%=10 */
         if(pz)pz=10-pz;
         CHECK_PZX10;

            /* Variante 2: Berechnung nach Methode 10 */
#if DEBUG>0
      case 2103:
         if(retvals){
            retvals->methode="A3b";
            retvals->pz_methode=2103;
         }
#endif
         pz = (kto[0]-'0') * 10
            + (kto[1]-'0') * 9
            + (kto[2]-'0') * 8
            + (kto[3]-'0') * 7
            + (kto[4]-'0') * 6
            + (kto[5]-'0') * 5
            + (kto[6]-'0') * 4
            + (kto[7]-'0') * 3
            + (kto[8]-'0') * 2;

         MOD_11_352;   /* pz%=11 */
         if(pz<=1)
            pz=0;
         else
            pz=11-pz;
         CHECK_PZ10;

/*  Berechnung nach der Methode A4 +§§§4 */
/*
 * ######################################################################
 * #              Berechnung nach der Methode A4                       #
 * ######################################################################
 * # Modulus 11, Gewichtung 2, 3, 4, 5, 6, 7, 0, 0, 0                   #
 * # Modulus 7,  Gewichtung 2, 3, 4, 5, 6, 7, 0, 0, 0                   #
 * # Modulus 11, Gewichtung 2, 3, 4, 5, 6, 0, 0, 0, 0                   #
 * # Modulus 7,  Gewichtung 2, 3, 4, 5, 6, 0, 0, 0, 0                   #
 * #                                                                    #
 * # Die Kontonummer ist einschließlich der Prüfziffer 10-stellig,      #
 * # ggf. ist die Kontonummer für die Prüfzifferberechnung durch        #
 * # linksbündige Auffüllung mit Nullen 10-stellig darzustellen. Zur    #
 * # Prüfung einer Kontonummer sind die folgenden Varianten zu          #
 * # rechnen. Dabei ist zu beachten, dass Kontonummern mit der          #
 * # Ziffernfolge 99 an den Stellen 3 und 4 (XX99XXXXXX) nur nach       #
 * # Variante 3 und ggf. 4 zu prüfen sind. Alle anderen Kontonummern    #
 * # sind nacheinander nach den Varianten 1, ggf. 2 und ggf. 4 zu       #
 * # prüfen.                                                            #
 * #                                                                    #
 * # Variante 1:                                                        #
 * # Modulus 11, Gewichtung 2, 3, 4, 5, 6, 7, 0, 0, 0                   #
 * #                                                                    #
 * # In die Prüfzifferberechnung werden nur die Stellen 4 bis 9         #
 * # einbezogen. Die Stelle 10 ist die Prüfziffer. Die weitere          #
 * # Berechnung erfolgt nach dem Verfahren 06.                          #
 * #                                                                    #
 * # Führt die Berechnung zu einem Fehler, ist nach Variante 2 zu       #
 * # prüfen.                                                            #
 * #                                                                    #
 * # Variante 2:                                                        #
 * # Modulus 7, Gewichtung 2, 3, 4, 5, 6, 7, 0, 0, 0                    #
 * #                                                                    #
 * # Die Stellen 4 bis 9 der Kontonummer werden von rechts nach links   #
 * # mit den Gewichten multipliziert. Die jeweiligen Produkte werden    #
 * # addiert. Die Summe ist durch 7 zu dividieren. Der verbleibende     #
 * # Rest wird vom Divisor (7) subtrahiert. Das Ergebnis ist die        #
 * # Prüfziffer (Stelle 10). Verbleibt nach der Division kein Rest,     #
 * # ist die Prüfziffer 0.                                              #
 * #                                                                    #
 * # Führt die Berechnung zu einem Fehler, ist nach Variante 4 zu       #
 * # prüfen.                                                            #
 * #                                                                    #
 * # Variante 3:                                                        #
 * # Modulus 11, Gewichtung 2, 3, 4, 5, 6, 0, 0, 0, 0                   #
 * #                                                                    #
 * # In die Prüfzifferberechnung werden nur die Stellen 5 bis 9         #
 * # einbezogen. Die Stelle 10 ist die Prüfziffer. Die weitere          #
 * # Berechnung erfolgt nach dem Verfahren 06.                          #
 * #                                                                    #
 * # Führt die Berechnung zu einem Fehler, ist nach Variante 4 zu       #
 * # prüfen.                                                            #
 * #                                                                    #
 * # Variante 4:                                                        #
 * # Modulus 11, Gewichtung 2, 3, 4, 5, 6, 0, 0, 0, 0                   #
 * # Modulus 7,  Gewichtung 2, 3, 4, 5, 6, 0, 0, 0, 0                   #
 * # Die Berechnung erfolgt nach der Methode 93.                        #
 * ######################################################################
 */

      case 104:
         if(*(kto+2)!='9' || *(kto+3)!='9'){

                /* Variante 1 */
#if DEBUG>0
      case 1104:
         if(retvals){
            retvals->methode="A4a";
            retvals->pz_methode=1104;
         }
#endif
         pz = (kto[3]-'0') * 7
            + (kto[4]-'0') * 6
            + (kto[5]-'0') * 5
            + (kto[6]-'0') * 4
            + (kto[7]-'0') * 3
            + (kto[8]-'0') * 2;

         MOD_11_176;   /* pz%=11 */
            if(pz<=1)
               pz=0;
            else
               pz=11-pz;
            CHECK_PZX10;

                /* Variante 2 */
#if DEBUG>0
      case 2104:
         if(retvals){
            retvals->methode="A4b";
            retvals->pz_methode=2104;
         }
#endif
         pz = (kto[3]-'0') * 7
            + (kto[4]-'0') * 6
            + (kto[5]-'0') * 5
            + (kto[6]-'0') * 4
            + (kto[7]-'0') * 3
            + (kto[8]-'0') * 2;

         MOD_7_224;   /* pz%=7 */
            if(pz)pz=7-pz;
            CHECK_PZX10;
         }
         else{ /* 3. und 4. Stelle sind 9 */

                /* Variante 3 */
#if DEBUG>0
      case 3104:
         if(retvals){
            retvals->methode="A4c";
            retvals->pz_methode=3104;
         }
#endif
         pz = (kto[4]-'0') * 6
            + (kto[5]-'0') * 5
            + (kto[6]-'0') * 4
            + (kto[7]-'0') * 3
            + (kto[8]-'0') * 2;

         MOD_11_176;   /* pz%=11 */
            if(pz<=1)
               pz=0;
            else
               pz=11-pz;
            CHECK_PZX10;
         }

            /* Variante 4: Methode 93, Variante 1 */
#if DEBUG>0
      case 4104:
         if(retvals){
            retvals->methode="A4d";
            retvals->pz_methode=4104;
         }
#endif
         if(*kto=='0' && *(kto+1)=='0' && *(kto+2)=='0' && *(kto+3)=='0'){   /* Fall b) */
            pz = (kto[4]-'0') * 6
               + (kto[5]-'0') * 5
               + (kto[6]-'0') * 4
               + (kto[7]-'0') * 3
               + (kto[8]-'0') * 2;

            p1=pz;
            MOD_11_176;   /* pz%=11 */
         }
         else{
            pz = (kto[0]-'0') * 6
               + (kto[1]-'0') * 5
               + (kto[2]-'0') * 4
               + (kto[3]-'0') * 3
               + (kto[4]-'0') * 2;

            kto[9]=kto[5];  /* Prüfziffer nach Stelle 10 */
            p1=pz;
            MOD_11_176;   /* pz%=11 */
         }
         if(pz<=1)
            pz=0;
         else
            pz=11-pz;
         CHECK_PZX10;

            /* Methode 93, Variante 2 */
#if DEBUG>0
#if DEBUG>0
      case 5104:
         if(retvals){
            retvals->methode="A4e";
            retvals->pz_methode=5104;
         }
#endif
         if(untermethode){ /* pz wurde noch nicht berechnet */
            if(*kto=='0' && *(kto+1)=='0' && *(kto+2)=='0' && *(kto+3)=='0'){   /* Fall b) */
               for(p1=0,ptr=kto+8,i=0;i<5;ptr--,i++)
                  p1+=(*ptr-'0')*w93[i];
            }
            else{
               for(p1=0,ptr=kto+4,i=0;i<5;ptr--,i++)
                  p1+=(*ptr-'0')*w93[i];
                  *(kto+9)= *(kto+5);  /* Prüfziffer nach Stelle 10 */
            }
         }
#endif
         pz=p1%7;
         if(pz)pz=7-pz;
         CHECK_PZ10;
         break;

/*  Berechnung nach der Methode A5 +§§§4 */
/*
 * ######################################################################
 * #              Berechnung nach der Methode A5                       #
 * ######################################################################
 * # Modulus 10, Gewichtung 2, 1, 2, 1, 2, 1, 2, 1, 2                   #
 * # Modulus 11, Gewichtung 2, 3, 4, 5, 6, 7, 8, 9, 10                  #
 * #                                                                    #
 * # Die Kontonummer ist einschließlich der Prüfziffer 10-stellig,      #
 * # ggf. ist die Kontonummer für die Prüfzifferberechnung durch        #
 * # linksbündige Auffüllung mit Nullen 10-stellig darzustellen.        #
 * #                                                                    #
 * # Variante 1: Gewichtung und Berechnung erfolgen nach der Methode    #
 * # 00. Führt die Berechnung nach Variante 1 zu einem                  #
 * # Prüfzifferfehler, so sind 10-stellige Konten mit einer 9 an        #
 * # Stelle 1 falsch, alle anderen Konten sind nach Variante 2 zu       #
 * # prüfen.                                                            #
 * #                                                                    #
 * # Variante 2: Gewichtung und Berechnung erfolgen nach der Methode 10.#
 * ######################################################################
 */
      case 105:
#if DEBUG>0
      case 1105:
         if(retvals){
            retvals->methode="A5a";
            retvals->pz_methode=1105;
         }
#endif
            /* Variante 1: Berechnung nach Methode 00 */
#ifdef __ALPHA
         pz = ((kto[0]<'5') ? (kto[0]-'0')*2 : (kto[0]-'0')*2-9)
            +  (kto[1]-'0')
            + ((kto[2]<'5') ? (kto[2]-'0')*2 : (kto[2]-'0')*2-9)
            +  (kto[3]-'0')
            + ((kto[4]<'5') ? (kto[4]-'0')*2 : (kto[4]-'0')*2-9)
            +  (kto[5]-'0')
            + ((kto[6]<'5') ? (kto[6]-'0')*2 : (kto[6]-'0')*2-9)
            +  (kto[7]-'0')
            + ((kto[8]<'5') ? (kto[8]-'0')*2 : (kto[8]-'0')*2-9);
#else
         pz =(kto[1]-'0')+(kto[3]-'0')+(kto[5]-'0')+(kto[7]-'0');
         if(kto[0]<'5')pz+=(kto[0]-'0')*2; else pz+=(kto[0]-'0')*2-9;
         if(kto[2]<'5')pz+=(kto[2]-'0')*2; else pz+=(kto[2]-'0')*2-9;
         if(kto[4]<'5')pz+=(kto[4]-'0')*2; else pz+=(kto[4]-'0')*2-9;
         if(kto[6]<'5')pz+=(kto[6]-'0')*2; else pz+=(kto[6]-'0')*2-9;
         if(kto[8]<'5')pz+=(kto[8]-'0')*2; else pz+=(kto[8]-'0')*2-9;
#endif

         MOD_10_80;   /* pz%=10 */
         if(pz)pz=10-pz;
         CHECK_PZX10;
         if(*kto=='9')return INVALID_KTO;

            /* Variante 2: Berechnung nach Methode 10 */
#if DEBUG>0
      case 2105:
         if(retvals){
            retvals->methode="A5b";
            retvals->pz_methode=2105;
         }
#endif
         pz = (kto[0]-'0') * 10
            + (kto[1]-'0') * 9
            + (kto[2]-'0') * 8
            + (kto[3]-'0') * 7
            + (kto[4]-'0') * 6
            + (kto[5]-'0') * 5
            + (kto[6]-'0') * 4
            + (kto[7]-'0') * 3
            + (kto[8]-'0') * 2;

         MOD_11_352;   /* pz%=11 */
         if(pz<=1)
            pz=0;
         else
            pz=11-pz;
         CHECK_PZ10;

/*  Berechnung nach der Methode A6 +§§§4 */
/*
 * ######################################################################
 * #              Berechnung nach der Methode A6                       #
 * ######################################################################
 * # Modulus 10, Gewichtung 2, 1, 2, 1, 2, 1, 2, 1, 2                   #
 * # Modulus 10, Gewichtung 3, 7, 1, 3, 7, 1, 3, 7, 1                   #
 * #                                                                    #
 * # Die Kontonummer ist einschließlich der Prüfziffer 10- stellig,     #
 * # ggf. ist die Kontonummer für die Prüfzifferberechnung durch        #
 * # linksbündige Auffüllung mit Nullen 10-stellig darzustellen. Die    #
 * # Stelle 10 ist die Prüfziffer.                                      #
 * #                                                                    #
 * # Sofern dann an der zweiten Stelle der Kontonummer eine 8 steht,    #
 * # erfolgen Gewichtung und Berechnung wie beim Verfahren 00.          #
 * #                                                                    #
 * # Bei allen Kontonummern, die keine 8 an der zweiten Stelle          #
 * # haben, erfolgen Gewichtung und Berechnung wie beim Verfahren 01.   #
 * ######################################################################
 */

      case 106:
         if(kto[1]=='8'){

                /* Variante 1 */
#if DEBUG>0
      case 1106:
         if(retvals){
            retvals->methode="A6a";
            retvals->pz_methode=1106;
         }
#endif
#ifdef __ALPHA
            pz = ((kto[0]<'5') ? (kto[0]-'0')*2 : (kto[0]-'0')*2-9)
               +  8
               + ((kto[2]<'5') ? (kto[2]-'0')*2 : (kto[2]-'0')*2-9)
               +  (kto[3]-'0')
               + ((kto[4]<'5') ? (kto[4]-'0')*2 : (kto[4]-'0')*2-9)
               +  (kto[5]-'0')
               + ((kto[6]<'5') ? (kto[6]-'0')*2 : (kto[6]-'0')*2-9)
               +  (kto[7]-'0')
               + ((kto[8]<'5') ? (kto[8]-'0')*2 : (kto[8]-'0')*2-9);
#else
            pz=8+(kto[3]-'0')+(kto[5]-'0')+(kto[7]-'0');
            if(kto[0]<'5')pz+=(kto[0]-'0')*2; else pz+=(kto[0]-'0')*2-9;
            if(kto[2]<'5')pz+=(kto[2]-'0')*2; else pz+=(kto[2]-'0')*2-9;
            if(kto[4]<'5')pz+=(kto[4]-'0')*2; else pz+=(kto[4]-'0')*2-9;
            if(kto[6]<'5')pz+=(kto[6]-'0')*2; else pz+=(kto[6]-'0')*2-9;
            if(kto[8]<'5')pz+=(kto[8]-'0')*2; else pz+=(kto[8]-'0')*2-9;
#endif
            MOD_10_80;   /* pz%=10 */
            if(pz)pz=10-pz;
            CHECK_PZ10;
         }
         else{

                /* Variante 2 */
#if DEBUG>0
      case 2106:
         if(retvals){
            retvals->methode="A6b";
            retvals->pz_methode=2106;
         }
#endif
            pz = (kto[0]-'0')
               + (kto[1]-'0') * 7
               + (kto[2]-'0') * 3
               + (kto[3]-'0')
               + (kto[4]-'0') * 7
               + (kto[5]-'0') * 3
               + (kto[6]-'0')
               + (kto[7]-'0') * 7
               + (kto[8]-'0') * 3;

            MOD_10_160;   /* pz%=10 */
            if(pz)pz=10-pz;
            CHECK_PZ10;
         }

/*  Berechnung nach der Methode A7 +§§§4 */
/*
 * ######################################################################
 * #              Berechnung nach der Methode A7                       #
 * ######################################################################
 * #  Modulus 10, Gewichtung 2, 1, 2, 1, 2, 1, 2, 1, 2                  #
 * #                                                                    #
 * #  Die Kontonummer ist einschließlich der Prüfziffer 10- stellig,    #
 * #  ggf. ist die Kontonummer für die Prüfzifferberechnung durch       #
 * #  linksbündige Auffüllung mit Nullen 10-stellig darzustellen.       #
 * #                                                                    #
 * #  Variante 1:                                                       #
 * #  Gewichtung und Berechnung erfolgen nach der Methode 00. Führt die #
 * #  Berechnung nach Variante 1 zu einem Prüfzifferfehler, ist nach    #
 * #  Variante 2 zu prüfen.                                             #
 * #                                                                    #
 * #  Variante 2:                                                       #
 * #  Gewichtung und Berechnung erfolgen nach der Methode 03.           #
 * ######################################################################
 */

      case 107:

                /* Variante 1 */
#if DEBUG>0
      case 1107:
         if(retvals){
            retvals->methode="A7a";
            retvals->pz_methode=1107;
         }
#endif
#ifdef __ALPHA
         pz = ((kto[0]<'5') ? (kto[0]-'0')*2 : (kto[0]-'0')*2-9)
            +  (kto[1]-'0')
            + ((kto[2]<'5') ? (kto[2]-'0')*2 : (kto[2]-'0')*2-9)
            +  (kto[3]-'0')
            + ((kto[4]<'5') ? (kto[4]-'0')*2 : (kto[4]-'0')*2-9)
            +  (kto[5]-'0')
            + ((kto[6]<'5') ? (kto[6]-'0')*2 : (kto[6]-'0')*2-9)
            +  (kto[7]-'0')
            + ((kto[8]<'5') ? (kto[8]-'0')*2 : (kto[8]-'0')*2-9);
#else
         pz=(kto[1]-'0')+(kto[3]-'0')+(kto[5]-'0')+(kto[7]-'0');
         if(kto[0]<'5')pz+=(kto[0]-'0')*2; else pz+=(kto[0]-'0')*2-9;
         if(kto[2]<'5')pz+=(kto[2]-'0')*2; else pz+=(kto[2]-'0')*2-9;
         if(kto[4]<'5')pz+=(kto[4]-'0')*2; else pz+=(kto[4]-'0')*2-9;
         if(kto[6]<'5')pz+=(kto[6]-'0')*2; else pz+=(kto[6]-'0')*2-9;
         if(kto[8]<'5')pz+=(kto[8]-'0')*2; else pz+=(kto[8]-'0')*2-9;
#endif
         MOD_10_80;   /* pz%=10 */
         if(pz)pz=10-pz;
         CHECK_PZX10;

                /* Variante 2 */
#if DEBUG>0
      case 2107:
         if(retvals){
            retvals->methode="A7b";
            retvals->pz_methode=2107;
         }
#endif
         pz = (kto[0]-'0') * 2
            + (kto[1]-'0')
            + (kto[2]-'0') * 2
            + (kto[3]-'0')
            + (kto[4]-'0') * 2
            + (kto[5]-'0')
            + (kto[6]-'0') * 2
            + (kto[7]-'0')
            + (kto[8]-'0') * 2;

         MOD_10_80;   /* pz%=10 */
         if(pz)pz=10-pz;
         CHECK_PZ10;


/*  Berechnung nach der Methode A8 +§§§4 */
/*
 * ######################################################################
 * #   Berechnung nach der Methode A8 (geändert zum 7.3.05)             #
 * ######################################################################
 * # Die Kontonummer ist durch linksbündige Nullenauffüllung 10-        #
 * # stellig darzustellen. Die 10. Stelle ist per Definition die        #
 * # Prüfziffer.                                                        #
 * #                                                                    #
 * # Variante 1:                                                        #
 * # Modulus 11, Gewichtung 2, 3, 4, 5, 6, 7                            #
 * # Die Stellen 4 bis 9 der Kontonummer werden von rechts nach links   #
 * # mit den Ziffern 2, 3, 4, 5, 6, 7 multipliziert. Die weitere        #
 * # Berechnung und die möglichen Ergebnisse entsprechen dem Verfahren  #
 * # 06. Führt die Berechnung nach Variante 1 zu einem Prüfziffer-      #
 * # fehler, so sind die Konten nach Variante 2 zu prüfen.              #
 * #                                                                    #
 * # Variante 2:                                                        #
 * # Modulus 10, Gewichtung 2, 1, 2, 1, 2, 1                            #
 * # Die Stellen 4 bis 9 der Kontonummer werden von rechts nach links   #
 * # mit den Ziffern 2, 1, 2, 1, 2, 1 multipliziert. Die weiter         #
 * # Berechnung und die möglichen Ergebnisse entsprechen dem Verfahren  #
 * # 00.                                                                #
 * #                                                                    #
 * # Ausnahme:                                                          #
 * # Ist nach linksbündiger Auffüllung mit Nullen auf 10 Stellen die    #
 * # 3. Stelle der Kontonummer = 9 (Sachkonten), so erfolgt die         #
 * # Berechnung gemäß der Ausnahme in Methode 51 mit den gleichen       #
 * # Ergebnissen und Testkontonummern.                                  #
 * ######################################################################
 */

      case 108:

                /* Ausnahme, Variante 1 */
         if(*(kto+2)=='9'){   /* Berechnung wie in Verfahren 51 */
#if DEBUG>0
      case 3108:
         if(retvals){
            retvals->methode="A8c";
            retvals->pz_methode=3108;
         }
#endif
            pz =         9 * 8
            + (kto[3]-'0') * 7
            + (kto[4]-'0') * 6
            + (kto[5]-'0') * 5
            + (kto[6]-'0') * 4
            + (kto[7]-'0') * 3
            + (kto[8]-'0') * 2;

            MOD_11_176;   /* pz%=11 */
            if(pz<=1)
               pz=0;
            else
               pz=11-pz;
            CHECK_PZX10;

            /* Ausnahme, Variante 2 */
#if DEBUG>0
      case 4108:
         if(retvals){
            retvals->methode="A8d";
            retvals->pz_methode=4108;
         }
#endif
               pz = (kto[0]-'0') * 10
               + (kto[1]-'0') * 9
               + (kto[2]-'0') * 8
               + (kto[3]-'0') * 7
               + (kto[4]-'0') * 6
               + (kto[5]-'0') * 5
               + (kto[6]-'0') * 4
               + (kto[7]-'0') * 3
               + (kto[8]-'0') * 2;

               MOD_11_352;   /* pz%=11 */
               if(pz<=1)
                  pz=0;
               else
                  pz=11-pz;
               CHECK_PZ10;
            }

#if DEBUG>0
      case 1108:
         if(retvals){
            retvals->methode="A8a";
            retvals->pz_methode=1108;
         }
#endif
         pz = (kto[3]-'0') * 7
            + (kto[4]-'0') * 6
            + (kto[5]-'0') * 5
            + (kto[6]-'0') * 4
            + (kto[7]-'0') * 3
            + (kto[8]-'0') * 2;

         MOD_11_176;   /* pz%=11 */
         if(pz<=1)
            pz=0;
         else
            pz=11-pz;
         CHECK_PZX10;

                /* Variante 2 */
#if DEBUG>0
      case 2108:
         if(retvals){
            retvals->methode="A8b";
            retvals->pz_methode=2108;
         }
#endif
#ifdef __ALPHA
         pz =  (kto[3]-'0')
            + ((kto[4]<'5') ? (kto[4]-'0')*2 : (kto[4]-'0')*2-9)
            +  (kto[5]-'0')
            + ((kto[6]<'5') ? (kto[6]-'0')*2 : (kto[6]-'0')*2-9)
            +  (kto[7]-'0')
            + ((kto[8]<'5') ? (kto[8]-'0')*2 : (kto[8]-'0')*2-9);
#else

         pz=(kto[3]-'0')+(kto[5]-'0')+(kto[7]-'0');
         if(kto[4]<'5')pz+=(kto[4]-'0')*2; else pz+=(kto[4]-'0')*2-9;
         if(kto[6]<'5')pz+=(kto[6]-'0')*2; else pz+=(kto[6]-'0')*2-9;
         if(kto[8]<'5')pz+=(kto[8]-'0')*2; else pz+=(kto[8]-'0')*2-9;
#endif
         MOD_10_80;   /* pz%=10 */
         if(pz)pz=10-pz;
         CHECK_PZ10;

/*  Berechnung nach der Methode A9 +§§§4 */
/*
 * ######################################################################
 * #              Berechnung nach der Methode A9                       #
 * ######################################################################
 * # Modulus 10, Gewichtung 3, 7, 1, 3, 7, 1, 3, 7, 1                   #
 * # Modulus 11, Gewichtung 2, 3, 4, 5, 6, 7, 2, 3, 4                   #
 * #                                                                    #
 * # Die Kontonummer ist einschließlich der Prüfziffer 10-stellig,      #
 * # ggf. ist die Kontonummer für die Prüfzifferberechnung durch        #
 * # linksbündige Auffüllung mit Nullen 10-stellig darzustellen.        #
 * #                                                                    #
 * # Variante 1:                                                        #
 * # Gewichtung und Berechnung erfolgen nach der Methode 01. Führt die  #
 * # Berechnung nach Variante 1 zu einem Prüfzifferfehler, so ist nach  #
 * # Variante 2 zu prüfen.                                              #
 * #                                                                    #
 * # Variante 2:                                                        #
 * # Gewichtung und Berechnung erfolgen nach der Methode 06.            #
 * ######################################################################
 */

      case 109:

            /* Variante 1: Berechnung nach Methode 01 */
#if DEBUG>0
      case 1109:
         if(retvals){
            retvals->methode="A9a";
            retvals->pz_methode=1109;
         }
#endif
         pz = (kto[0]-'0')
            + (kto[1]-'0') * 7
            + (kto[2]-'0') * 3
            + (kto[3]-'0')
            + (kto[4]-'0') * 7
            + (kto[5]-'0') * 3
            + (kto[6]-'0')
            + (kto[7]-'0') * 7
            + (kto[8]-'0') * 3;

         MOD_10_160;   /* pz%=10 */
         if(pz)pz=10-pz;
         CHECK_PZX10;

            /* Variante 2: Berechnung nach Methode 06 */
#if DEBUG>0
      case 2109:
         if(retvals){
            retvals->methode="A9b";
            retvals->pz_methode=2109;
         }
#endif
         pz = (kto[0]-'0') * 4
            + (kto[1]-'0') * 3
            + (kto[2]-'0') * 2
            + (kto[3]-'0') * 7
            + (kto[4]-'0') * 6
            + (kto[5]-'0') * 5
            + (kto[6]-'0') * 4
            + (kto[7]-'0') * 3
            + (kto[8]-'0') * 2;

         MOD_11_176;   /* pz%=11 */
         if(pz<=1)
            pz=0;
         else
            pz=11-pz;
         CHECK_PZ10;

/* Berechnungsmethoden B0 bis B9 +§§§3
   Berechnung nach der Methode B0 +§§§4 */
/*
 * ######################################################################
 * #              Berechnung nach der Methode B0                       #
 * ######################################################################
 * #                                                                    #
 * # Die Kontonummern sind immer 10-stellig. Kontonummern (ohne         #
 * # führende Nullen gezählt) mit 9 oder weniger Stellen sind falsch.   #
 * # Kontonummern mit 8 an der ersten Stelle sind ebenfalls falsch.     #
 * # Die weitere Verfahrensweise richtet sich nach der 8. Stelle der    #
 * # Kontonummer:                                                       #
 * #                                                                    #
 * # Variante 1                                                         #
 * #                                                                    #
 * # Für Kontonummern mit einer 1, 2, 3, oder 6 an der 8. Stelle gilt   #
 * # das Verfahren 09 (Keine Prüfzifferberechnung, alle Kontonummern    #
 * # sind richtig).                                                     #
 * #                                                                    #
 * # Variante 2                                                         #
 * #                                                                    #
 * # Modulus 11, Gewichtung 2, 3, 4, 5, 6, 7, 2, 3, 4                   #
 * #             (von rechts beginnend)                                 #
 * #                                                                    #
 * # Für Kontonummern mit einer 0, 4, 5, 7, 8 oder 9 an der 8. Stelle   #
 * # erfolgen Gewichtung und Berechnung wie beim Verfahren 06.          #
 * ######################################################################
 */

      case 110:
#if DEBUG>0
         if(retvals){
            retvals->methode="B0";
            retvals->pz_methode=110;
         }
#endif
         if(kto[0]=='0' || kto[0]=='8')return INVALID_KTO;

            /* Variante 1 */
         if(kto[7]=='1' || kto[7]=='2' || kto[7]=='3' || kto[7]=='6'){
#if DEBUG>0
      case 1110:
         if(retvals){
            retvals->methode="B0a";
            retvals->pz_methode=1110;
         }
#endif
            return OK_NO_CHK;
         }

            /* Variante 2 */
#if DEBUG>0
      case 2110:
         if(retvals){
            retvals->methode="B0b";
            retvals->pz_methode=2110;
         }
#endif
         pz = (kto[0]-'0') * 4
            + (kto[1]-'0') * 3
            + (kto[2]-'0') * 2
            + (kto[3]-'0') * 7
            + (kto[4]-'0') * 6
            + (kto[5]-'0') * 5
            + (kto[6]-'0') * 4
            + (kto[7]-'0') * 3
            + (kto[8]-'0') * 2;

         MOD_11_176;   /* pz%=11 */
         if(pz<=1)
            pz=0;
         else
            pz=11-pz;
         CHECK_PZ10;

/*  Berechnung nach der Methode B1 +§§§4 */
/*
 * ######################################################################
 * #              Berechnung nach der Methode B1                       #
 * ######################################################################
 * # Die Kontonummer ist einschließlich der Prüfziffer 10-stellig,      #
 * # ggf. ist die Kontonummer für die Prüfzifferberechnung durch        #
 * # linksbündige Auffüllung mit Nullen 10-stellig darzustellen.        #
 * #                                                                    #
 * # Variante 1:                                                        #
 * # Modulus 10, Gewichtung 7, 3, 1, 7, 3, 1, 7, 3, 1                   #
 * # Gewichtung und Berechnung erfolgen nach der Methode 05. Führt die  #
 * # Berechnung nach Variante 1 zu einem Prüfzifferfehler, so ist nach  #
 * # Variante 2 zu prüfen.                                              #
 * #                                                                    #
 * # Variante 2:                                                        #
 * # Modulus 10, Gewichtung 3, 7, 1, 3, 7, 1, 3, 7, 1                   #
 * # Gewichtung und Berechnung erfolgen nach der Methode 01.            #
 * ######################################################################
 */

      case 111:

            /* Variante 1: Berechnung nach Methode 05 */
#if DEBUG>0
      case 1111:
         if(retvals){
            retvals->methode="B1a";
            retvals->pz_methode=1111;
         }
#endif
         pz = (kto[0]-'0')
            + (kto[1]-'0') * 3
            + (kto[2]-'0') * 7
            + (kto[3]-'0')
            + (kto[4]-'0') * 3
            + (kto[5]-'0') * 7
            + (kto[6]-'0')
            + (kto[7]-'0') * 3
            + (kto[8]-'0') * 7;

         MOD_10_160;   /* pz%=10 */
         if(pz)pz=10-pz;
         CHECK_PZX10;

            /* Variante 2: Berechnung nach Methode 01 */
#if DEBUG>0
      case 2111:
         if(retvals){
            retvals->methode="B1b";
            retvals->pz_methode=2111;
         }
#endif
         pz = (kto[0]-'0')
            + (kto[1]-'0') * 7
            + (kto[2]-'0') * 3
            + (kto[3]-'0')
            + (kto[4]-'0') * 7
            + (kto[5]-'0') * 3
            + (kto[6]-'0')
            + (kto[7]-'0') * 7
            + (kto[8]-'0') * 3;

         MOD_10_160;   /* pz%=10 */
         if(pz)pz=10-pz;
         CHECK_PZ10;

/*  Berechnung nach der Methode B2 +§§§4 */
/*
 * ######################################################################
 * #              Berechnung nach der Methode B2                        #
 * ######################################################################
 * # Die Kontonummer ist einschließlich der Prüfziffer 10-stellig,      #
 * # ggf. ist die Kontonummer für die Prüfzifferberechnung durch        #
 * # linksbündige Auffüllung mit Nullen 10-stellig darzustellen.        #
 * #                                                                    #
 * # Variante 1:                                                        #
 * # Modulus 11, Gewichtung 2, 3, 4, 5, 6, 7, 8, 9, 2                   #
 * # Kontonummern, die an der 1. Stelle von links der 10- stelligen     #
 * # Kontonummer den Wert 0 bis 7 beinhalten, sind nach der Methode 02  #
 * # zu rechnen.                                                        #
 * #                                                                    #
 * # Variante 2:                                                        #
 * # Modulus 10, Gewichtung 2, 1, 2, 1, 2, 1, 2, 1, 2                   #
 * # Kontonummern, die an der 1. Stelle von links der 10- stelligen     #
 * # Kontonummer den Wert 8 oder 9 beinhalten, sind nach der Methode    #
 * # 00 zu rechnen.                                                     #
 * ######################################################################
 */

      case 112:

            /* Variante 1: Methode 02 */
         if(*kto<'8'){
#if DEBUG>0
      case 1112:
         if(retvals){
            retvals->methode="B2a";
            retvals->pz_methode=1112;
         }
#endif
            pz = (kto[0]-'0') * 2
               + (kto[1]-'0') * 9
               + (kto[2]-'0') * 8
               + (kto[3]-'0') * 7
               + (kto[4]-'0') * 6
               + (kto[5]-'0') * 5
               + (kto[6]-'0') * 4
               + (kto[7]-'0') * 3
               + (kto[8]-'0') * 2;

            MOD_11_352;   /* pz%=11 */
            if(pz)pz=11-pz;
            INVALID_PZ10;
            CHECK_PZ10;
         }
         else{
#if DEBUG>0
      case 2112:
         if(retvals){
            retvals->methode="B2b";
            retvals->pz_methode=2112;
         }
#endif
               /* Variante 2: Methode 00 */
#ifdef __ALPHA
            pz = ((kto[0]<'5') ? (kto[0]-'0')*2 : (kto[0]-'0')*2-9)
               +  (kto[1]-'0')
               + ((kto[2]<'5') ? (kto[2]-'0')*2 : (kto[2]-'0')*2-9)
               +  (kto[3]-'0')
               + ((kto[4]<'5') ? (kto[4]-'0')*2 : (kto[4]-'0')*2-9)
               +  (kto[5]-'0')
               + ((kto[6]<'5') ? (kto[6]-'0')*2 : (kto[6]-'0')*2-9)
               +  (kto[7]-'0')
               + ((kto[8]<'5') ? (kto[8]-'0')*2 : (kto[8]-'0')*2-9);
#else
            pz=(kto[1]-'0')+(kto[3]-'0')+(kto[5]-'0')+(kto[7]-'0');
            if(kto[0]<'5')pz+=(kto[0]-'0')*2; else pz+=(kto[0]-'0')*2-9;
            if(kto[2]<'5')pz+=(kto[2]-'0')*2; else pz+=(kto[2]-'0')*2-9;
            if(kto[4]<'5')pz+=(kto[4]-'0')*2; else pz+=(kto[4]-'0')*2-9;
            if(kto[6]<'5')pz+=(kto[6]-'0')*2; else pz+=(kto[6]-'0')*2-9;
            if(kto[8]<'5')pz+=(kto[8]-'0')*2; else pz+=(kto[8]-'0')*2-9;
#endif
            MOD_10_80;   /* pz%=10 */
            if(pz)pz=10-pz;
            CHECK_PZ10;
         }

/*  Berechnung nach der Methode B3 +§§§4 */
/*
 * ######################################################################
 * #              Berechnung nach der Methode B3                        #
 * ######################################################################
 * # Die Kontonummer ist einschließlich der Prüfziffer 10-stellig, ggf. #
 * # ist die Kontonummer für die Prüfzifferberechnung durch             #
 * # linksbündige Auffüllung mit Nullen 10-stellig darzustellen.        #
 * #                                                                    #
 * # Variante 1:                                                        #
 * #                                                                    #
 * # Modulus 11, Gewichtung 2, 3, 4, 5, 6, 7                            #
 * #                                                                    #
 * # Die Kontonummer ist 10-stellig. Kontonummern, die an der 1. Stelle #
 * # von links der 10-stelligen Kontonummer den Wert bis 8 beinhalten   #
 * # sind nach der Methode 32 zu rechen.                                #
 * #                                                                    #
 * #                                                                    #
 * # Variante 2:                                                        #
 * #                                                                    #
 * # Modulus 11, Gewichtung 2, 3, 4, 5, 6, 7, 2, 3, 4                   #
 * #                                                                    #
 * # Kontonummern, die an der 1. Stelle von links der 10- stelligen     #
 * # Kontonummer den Wert 9 beinhalten sind nach der Methode 06 zu      #
 * # rechen.                                                            #
 * ######################################################################
 */

      case 113:

            /* Variante 1: Methode 32 */
         if(*kto<'9'){
#if DEBUG>0
      case 1113:
         if(retvals){
            retvals->methode="B3a";
            retvals->pz_methode=1113;
         }
#endif
            pz = (kto[3]-'0') * 7
               + (kto[4]-'0') * 6
               + (kto[5]-'0') * 5
               + (kto[6]-'0') * 4
               + (kto[7]-'0') * 3
               + (kto[8]-'0') * 2;

            MOD_11_176;   /* pz%=11 */
            if(pz<=1)
               pz=0;
            else
               pz=11-pz;
            CHECK_PZ10;
         }
         else{

            /* Variante 2: Methode 06 */
#if DEBUG>0
      case 2113:
         if(retvals){
            retvals->methode="B3b";
            retvals->pz_methode=2113;
         }
#endif
            pz = (kto[0]-'0') * 4
               + (kto[1]-'0') * 3
               + (kto[2]-'0') * 2
               + (kto[3]-'0') * 7
               + (kto[4]-'0') * 6
               + (kto[5]-'0') * 5
               + (kto[6]-'0') * 4
               + (kto[7]-'0') * 3
               + (kto[8]-'0') * 2;

            MOD_11_176;   /* pz%=11 */
            if(pz<=1)
               pz=0;
            else
               pz=11-pz;
            CHECK_PZ10;
         }

/*  Berechnung nach der Methode B4 +§§§4 */
/*
 * ######################################################################
 * #              Berechnung nach der Methode B4                        #
 * ######################################################################
 * # Die Kontonummer ist einschließlich der Prüfziffer 10-stellig,      #
 * # ggf. ist die Kontonummer für die Prüfzifferberechnung durch        #
 * # linksbündige Auffüllung mit Nullen 10-stellig darzustellen.        #
 * #                                                                    #
 * # Variante 1:                                                        #
 * #                                                                    #
 * # Modulus 10, Gewichtung 2, 1, 2, 1, 2, 1, 2, 1, 2                   #
 * # Kontonummern, die an der 1. Stelle von links der 10-stelligen      #
 * # Kontonummer den Wert 9 beinhalten, sind nach der Methode 00 zu     #
 * # rechnen.                                                           #
 * #                                                                    #
 * # Variante 2:                                                        #
 * #                                                                    #
 * # Modulus 11, Gewichtung 2, 3, 4, 5, 6, 7, 8, 9, 10                  #
 * # Kontonummern, die an der 1. Stelle von links der 10-stelligen      #
 * # Kontonummer den Wert 0 bis 8 beinhalten, sind nach der Methode     #
 * # 02 zu rechnen.                                                     #
 * ######################################################################
 */

      case 114:

         if(*kto=='9'){
            /* Variante 1: Methode 00 */
#if DEBUG>0
      case 1114:
         if(retvals){
            retvals->methode="B4a";
            retvals->pz_methode=1114;
         }
#endif

#ifdef __ALPHA
            pz = ((kto[0]<'5') ? (kto[0]-'0')*2 : (kto[0]-'0')*2-9)
               +  (kto[1]-'0')
               + ((kto[2]<'5') ? (kto[2]-'0')*2 : (kto[2]-'0')*2-9)
               +  (kto[3]-'0')
               + ((kto[4]<'5') ? (kto[4]-'0')*2 : (kto[4]-'0')*2-9)
               +  (kto[5]-'0')
               + ((kto[6]<'5') ? (kto[6]-'0')*2 : (kto[6]-'0')*2-9)
               +  (kto[7]-'0')
               + ((kto[8]<'5') ? (kto[8]-'0')*2 : (kto[8]-'0')*2-9);
#else
            pz=(kto[1]-'0')+(kto[3]-'0')+(kto[5]-'0')+(kto[7]-'0');
            if(kto[0]<'5')pz+=(kto[0]-'0')*2; else pz+=(kto[0]-'0')*2-9;
            if(kto[2]<'5')pz+=(kto[2]-'0')*2; else pz+=(kto[2]-'0')*2-9;
            if(kto[4]<'5')pz+=(kto[4]-'0')*2; else pz+=(kto[4]-'0')*2-9;
            if(kto[6]<'5')pz+=(kto[6]-'0')*2; else pz+=(kto[6]-'0')*2-9;
            if(kto[8]<'5')pz+=(kto[8]-'0')*2; else pz+=(kto[8]-'0')*2-9;
#endif
            MOD_10_80;   /* pz%=10 */
            if(pz)pz=10-pz;
            CHECK_PZ10;
         }
         else{

            /* Variante 2: Methode 02 */
#if DEBUG>0
      case 2114:
         if(retvals){
            retvals->methode="B4b";
            retvals->pz_methode=2114;
         }
#endif
            pz = (kto[0]-'0') * 10
               + (kto[1]-'0') * 9
               + (kto[2]-'0') * 8
               + (kto[3]-'0') * 7
               + (kto[4]-'0') * 6
               + (kto[5]-'0') * 5
               + (kto[6]-'0') * 4
               + (kto[7]-'0') * 3
               + (kto[8]-'0') * 2;

            MOD_11_352;   /* pz%=11 */
            if(pz)pz=11-pz;
            INVALID_PZ10;
            CHECK_PZ10;
         }

/*  Berechnung nach der Methode B5 +§§§4 */
/*
 * ######################################################################
 * #              Berechnung nach der Methode B5                        #
 * ######################################################################
 * # Die Kontonummer ist einschließlich der Prüfziffer 10-stellig,      #
 * # ggf. ist die Kontonummer für die Prüfzifferberechnung durch        #
 * # linksbündige Auffüllung mit Nullen 10-stellig darzustellen.        #
 * #                                                                    #
 * # Variante 1:                                                        #
 * # Modulus 10, Gewichtung 7, 3, 1 ,7, 3, 1, 7, 3, 1                   #
 * # Die Gewichtung entspricht der Methode 05. Die Berechnung           #
 * # entspricht der Methode 01. Führt die Berechnung nach der Variante  #
 * # 1 zu einem Prüfzifferfehler, so sind Kontonummern, die an der 1.   #
 * # Stelle von links der 10-stelligen Kontonummer den Wert 8 oder 9    #
 * # beinhalten, falsch. Alle anderen Kontonummern sind nach der        #
 * # Variante 2 zu prüfen.                                              #
 * #                                                                    #
 * # Variante 2:                                                        #
 * # Modulus 10, Gewichtung 2, 1, 2, 1, 2, 1, 2, 1, 2                   #
 * # Gewichtung und Berechnung erfolgen nach der Methode  00.           #
 * ######################################################################
 */

      case 115:
#if DEBUG>0
      case 1115:
         if(retvals){
            retvals->methode="B5a";
            retvals->pz_methode=1115;
         }
#endif
         pz = (kto[0]-'0')
            + (kto[1]-'0') * 3
            + (kto[2]-'0') * 7
            + (kto[3]-'0')
            + (kto[4]-'0') * 3
            + (kto[5]-'0') * 7
            + (kto[6]-'0')
            + (kto[7]-'0') * 3
            + (kto[8]-'0') * 7;

         MOD_10_160;   /* pz%=10 */
         if(pz)pz=10-pz;
         CHECK_PZX10;
         if(*kto>'7')return FALSE;

#if DEBUG>0
      case 2115:
         if(retvals){
            retvals->methode="B5b";
            retvals->pz_methode=2115;
         }
#endif
#ifdef __ALPHA
         pz = ((kto[0]<'5') ? (kto[0]-'0')*2 : (kto[0]-'0')*2-9)
            +  (kto[1]-'0')
            + ((kto[2]<'5') ? (kto[2]-'0')*2 : (kto[2]-'0')*2-9)
            +  (kto[3]-'0')
            + ((kto[4]<'5') ? (kto[4]-'0')*2 : (kto[4]-'0')*2-9)
            +  (kto[5]-'0')
            + ((kto[6]<'5') ? (kto[6]-'0')*2 : (kto[6]-'0')*2-9)
            +  (kto[7]-'0')
            + ((kto[8]<'5') ? (kto[8]-'0')*2 : (kto[8]-'0')*2-9);
#else
         pz=(kto[1]-'0')+(kto[3]-'0')+(kto[5]-'0')+(kto[7]-'0');
         if(kto[0]<'5')pz+=(kto[0]-'0')*2; else pz+=(kto[0]-'0')*2-9;
         if(kto[2]<'5')pz+=(kto[2]-'0')*2; else pz+=(kto[2]-'0')*2-9;
         if(kto[4]<'5')pz+=(kto[4]-'0')*2; else pz+=(kto[4]-'0')*2-9;
         if(kto[6]<'5')pz+=(kto[6]-'0')*2; else pz+=(kto[6]-'0')*2-9;
         if(kto[8]<'5')pz+=(kto[8]-'0')*2; else pz+=(kto[8]-'0')*2-9;
#endif
         MOD_10_80;   /* pz%=10 */
         if(pz)pz=10-pz;
         CHECK_PZ10;


/*  Berechnung nach der Methode B6 +§§§4 */
/*
 * ######################################################################
 * #              Berechnung nach der Methode B6                        #
 * ######################################################################
 * # Variante 1:                                                        #
 * # Modulus 11, Gewichtung 2,3,4,5,6,7,8,9,3                           #
 * # Kontonummern, die an der 1. Stelle der 10-stelligen Kontonummer    #
 * # den Wert 1-9 beinhalten, sind nach der Methode 20 zu prüfen.       #
 * # Alle anderen Kontonummern sind nach der Variante 2 zu prüfen.      #
 * #                                                                    #
 * # Variante 2:                                                        #
 * # Modulus 11, Gewichtung 2,4,8,5,10,9,7,3,6,1,2,4                    #
 * # Die Berechnung erfolgt nach der Methode 53.                        #
 * ######################################################################
 */

      case 116:

         if(kto[0]>'0'){
#if DEBUG>0
      case 1116:
         if(retvals){
            retvals->methode="B6a";
            retvals->pz_methode=1116;
         }
#endif
            pz = (kto[0]-'0') * 3
               + (kto[1]-'0') * 9
               + (kto[2]-'0') * 8
               + (kto[3]-'0') * 7
               + (kto[4]-'0') * 6
               + (kto[5]-'0') * 5
               + (kto[6]-'0') * 4
               + (kto[7]-'0') * 3
               + (kto[8]-'0') * 2;

            MOD_11_352;   /* pz%=11 */
            if(pz<=1)
               pz=0;
            else
               pz=11-pz;
            CHECK_PZ10;
         }
         else{
#if DEBUG>0
      case 2116:
         if(retvals){
            retvals->methode="B6b";
            retvals->pz_methode=2116;
         }
#endif
            if(!x_blz){
               ok=OK_TEST_BLZ_USED;
               x_blz="80053762";
            }
            else
               ok=OK;

               /* Generieren der Konto-Nr. des ESER-Altsystems */
            for(ptr=kto;*ptr=='0';ptr++);
            if(*kto!='0' || *(kto+1)=='0'){  /* Kto-Nr. muß neunstellig sein */
#if DEBUG>0
#endif
               return INVALID_KTO;
            }
            kto_alt[0]=x_blz[4];
            kto_alt[1]=x_blz[5];
            kto_alt[2]=kto[2];         /* T-Ziffer */
            kto_alt[3]=x_blz[7];
            kto_alt[4]=kto[1];
            kto_alt[5]=kto[3];
            for(ptr=kto+4;*ptr=='0' && *ptr;ptr++);
            for(dptr=kto_alt+6;(*dptr= *ptr++);dptr++);
            kto=kto_alt;
            p1=kto_alt[5];   /* Prüfziffer merken */
            kto_alt[5]='0';
            for(pz=0,ptr=kto_alt+strlen(kto_alt)-1,i=0;ptr>=kto_alt;ptr--,i++)
               pz+=(*ptr-'0')*w52[i];
            kto_alt[5]=p1;   /* Prüfziffer zurückschreiben */
            pz=pz%11;
            p1=w52[i-6];

               /* passenden Faktor suchen */
            tmp=pz;
            for(i=0;i<10;i++){
               pz=tmp+p1*i;
               MOD_11_88;
               if(pz==10)break;
            }
            pz=i; /* Prüfziffer ist der verwendete Faktor des Gewichtes */
#if DEBUG>0
            if(retvals)retvals->pz=pz; 
#endif
            INVALID_PZ10;
            if(*(kto+5)-'0'==pz)
               return ok;
            else
               return FALSE;
         }

/*  Berechnung nach der Methode B7 +§§§4 */
/*
 * ######################################################################
 * #              Berechnung nach der Methode B7                        #
 * ######################################################################
 * #                                                                    #
 * # Die Kontonummer ist einschließlich der Prüfziffer 10-stellig,      #
 * # ggff. ist die Kontonummer für die Prüfzifferberechnung durch       #
 * # linksbündige Auffüllung mit Nullen 10-stellig darzustellen. Die    #
 * # 10. Stelle der Kontonummer ist die Prüfziffer.                     #
 * #                                                                    #
 * # Variante 1: Modulus 10, Gewichtung 3, 7, 1, 3, 7, 1, 3, 7, 1       #
 * # Kontonummern der Kontenkreise 0001000000 bis 0005999999 sowie      #
 * # 0700000000 bis 0899999999 sind nach der Methode (Kennziffer) 01    #
 * # zu prüfen. Führt die Berechnung nach der Variante 1 zu einem       #
 * # Prüfzifferfehler, so ist die Kontonummer falsch.                   #
 * #                                                                    #
 * # Variante 2: Für alle anderen Kontonummern gilt die Methode 09      #
 * # (keine Prüfzifferberechnung).                                      #
 * ######################################################################
 */

      case 117:
#if DEBUG>0
      case 1117:
         if(retvals){
            retvals->methode="B7a";
            retvals->pz_methode=1117;
         }
#endif
         if(kto[0]=='0' && ((kto[1]=='7' || kto[1]=='8')
                 || (kto[1]=='0' && kto[2]=='0' && kto[3]>='1' && kto[3]<'6'))){
            pz = (kto[0]-'0')
               + (kto[1]-'0') * 7
               + (kto[2]-'0') * 3
               + (kto[3]-'0')
               + (kto[4]-'0') * 7
               + (kto[5]-'0') * 3
               + (kto[6]-'0')
               + (kto[7]-'0') * 7
               + (kto[8]-'0') * 3;

            MOD_10_160;   /* pz%=10 */
            if(pz)pz=10-pz;
            CHECK_PZ10;
         }
         else{
#if DEBUG>0
      case 2117:
         if(retvals){
            retvals->methode="B7b";
            retvals->pz_methode=2117;
         }
#endif
            return OK_NO_CHK;
         }

/*  Berechnung nach der Methode B8 +§§§4 */
/*
 * ######################################################################
 * #              Berechnung nach der Methode B8                        #
 * ######################################################################
 * # Die Kontonummer ist einschließlich der Prüfziffer 10-stellig,      #
 * # ggf. ist die Kontonummer für die Prüfzifferberechnung durch        #
 * # linksbündige Auffüllung mit Nullen 10-stellig darzustellen. Die    #
 * # 10. Stelle der Kontonummer ist die Prüfziffer.                     #
 * #                                                                    #
 * # Variante 1:                                                        #
 * # Modulus 11, Gewichtung 2, 3, 4, 5, 6, 7, 8, 9, 3 (modifiziert)     #
 * #                                                                    #
 * # Die Berechnung und mögliche Ergebnisse entsprechen der Methode     #
 * # 20. Führt die Berechnung nach Variante 1 zu einem                  #
 * # Prüfzifferfehler, so ist nach Variante 2 zu prüfen.                #
 * #                                                                    #
 * # Variante 2: Modulus 10, iterierte Transformation.                  #
 * # Die Berechnung und mögliche Ergebnisse entsprechen der Methode 29. #
 * ######################################################################
 */

      case 118:
#if DEBUG>0
      case 1118:
         if(retvals){
            retvals->methode="B8a";
            retvals->pz_methode=1118;
         }
#endif
         pz = (kto[0]-'0') * 3
            + (kto[1]-'0') * 9
            + (kto[2]-'0') * 8
            + (kto[3]-'0') * 7
            + (kto[4]-'0') * 6
            + (kto[5]-'0') * 5
            + (kto[6]-'0') * 4
            + (kto[7]-'0') * 3
            + (kto[8]-'0') * 2;

         MOD_11_352;   /* pz%=11 */
         if(pz<=1)
            pz=0;
         else
            pz=11-pz;
         CHECK_PZX10;

#if DEBUG>0
      case 2118:
         if(retvals){
            retvals->methode="B8b";
            retvals->pz_methode=2118;
         }
#endif
         pz = m10h_digits[0][(unsigned int)(kto[0]-'0')]
            + m10h_digits[3][(unsigned int)(kto[1]-'0')]
            + m10h_digits[2][(unsigned int)(kto[2]-'0')]
            + m10h_digits[1][(unsigned int)(kto[3]-'0')]
            + m10h_digits[0][(unsigned int)(kto[4]-'0')]
            + m10h_digits[3][(unsigned int)(kto[5]-'0')]
            + m10h_digits[2][(unsigned int)(kto[6]-'0')]
            + m10h_digits[1][(unsigned int)(kto[7]-'0')]
            + m10h_digits[0][(unsigned int)(kto[8]-'0')];
         MOD_10_80;   /* pz%=10 */
         if(pz)pz=10-pz;
         CHECK_PZ10;

/*  Berechnung nach der Methode B9 +§§§4 */
/*
 * ######################################################################
 * #              Berechnung nach der Methode B9                        #
 * ######################################################################
 * #                                                                    #
 * # Die Kontonummer ist einschließlich der Prüfziffer 10-stellig,      #
 * # ggf. ist die Kontonummer für die Prüfzifferberechnung durch        #
 * # linksbündige Auffüllung mit Nullen 10-stellig darzustellen.        #
 * #                                                                    #
 * # Kontonummern mit weniger als zwei oder mehr als drei führenden     #
 * # Nullen sind falsch. Die Kontonummern mit zwei führenden Nullen     #
 * # sind nach Variante 1, mit drei führenden Nullen nach Variante 2    #
 * # zu prüfen.                                                         #
 * #                                                                    #
 * # Variante 1:                                                        #
 * # Modulus (11,10), Gewichtung 1, 3, 2, 1, 3, 2, 1                    #
 * # Die für die Berechnung relevanten Stellen der Kontonummer befinden #
 * # sich - von links nach rechts gelesen ­ in den Stellen 3-9 (die     #
 * # Prüfziffer ist in Stelle 10). Sie sind von rechts nach links       #
 * # mit den zugehörigen Gewichtungsfaktoren zu multiplizieren.         #
 * #                                                                    #
 * # Zum jeweiligen Produkt ist der zugehörige Gewichtungsfaktor zu     #
 * # addieren. Das jeweilige Ergebnis ist durch 11 zu dividieren. Die   #
 * # sich aus der Division ergebenden Reste sind zu summieren. Diese    #
 * # Summe ist durch 10 zu dividieren. Der Rest ist die berechnete      #
 * # Prüfziffer.                                                        #
 * #                                                                    #
 * # Führt die Berechnung zu einem Prüfzifferfehler, so ist die         #
 * # berechnete Prüfziffer um 5 zu erhöhen und erneut zu prüfen. Ist    #
 * # die Prüfziffer größer oder gleich 10, ist 10 abzuziehen und das    #
 * # Ergebnis ist dann die Prüfziffer.                                  #
 * #                                                                    #
 * # Variante 2:                                                        #
 * # Modulus 11, Gewichtung 1, 2, 3, 4, 5, 6                            #
 * # Die für die Berechnung relevanten Stellen der Kontonummer          #
 * # befinden sich - von links nach rechts gelesen - in den Stellen     #
 * # 4-9 (die Prüfziffer ist in Stelle 10). Sie sind von rechts nach    #
 * # links mit den zugehörigen Gewichtungsfaktoren zu multiplizieren.   #
 * # Die Summe dieser Produkte ist zu bilden, und das erzielte          #
 * # Ergebnis ist durch 11 zu dividieren. Der Rest ist die berechnete   #
 * # Prüfziffer.                                                        #
 * #                                                                    #
 * # Führt die Berechnung zu einem Prüfzifferfehler, so ist die         #
 * # berechnete Prüfziffer um 5 zu erhöhen und erneut zu prüfen. Ist    #
 * # die Prüfziffer größer oder gleich 10, ist 10 abzuziehen und das    #
 * # Ergebnis ist dann die Prüfziffer.                                  #
 * ######################################################################
 */

      case 119:
#if DEBUG>0
         if(retvals){
            retvals->methode="B9";
            retvals->pz_methode=119;
         }
#endif
         if(*kto!='0' || kto[1]!='0')return INVALID_KTO;
         if(*kto=='0' && kto[1]=='0' && kto[2]=='0' && kto[3]=='0')return INVALID_KTO;
         if(kto[2]!='0'){
#if DEBUG>0
      case 1119:
         if(retvals){
            retvals->methode="B9a";
            retvals->pz_methode=1119;
         }
#endif
            pz  = (kto[2]-'0') * 1 + 1;   /* Maximum von pz1 ist 9*1+1=10 -> kann direkt genommen werden */

            pz1 = (kto[3]-'0') * 2 + 2;   /* Maximum von pz1 ist 9*2+2=20 -> nur ein Test auf >=11 */
            if(pz1>=11)
               pz+=pz1-11;
            else
               pz+=pz1;

            pz1 = (kto[4]-'0') * 3 + 3;   /* Maximum von pz1 ist 9*3+3=30 -> zwei Tests auf >=22 und >=11 nötig */
            if(pz1>=22)
               pz+=pz1-22;
            else if(pz1>=11)
               pz+=pz1-11;
            else
               pz+=pz1;

            pz += (kto[5]-'0') * 1 + 1;   /* Maximum von pz1 ist 9*1+1=10 -> kann direkt genommen werden */

            pz1 = (kto[6]-'0') * 2 + 2;   /* Maximum von pz1 ist 9*2+2=20 -> nur ein Test auf >=11 */
            if(pz1>=11)
               pz+=pz1-11;
            else
               pz+=pz1;

            pz1 = (kto[7]-'0') * 3 + 3;   /* Maximum von pz1 ist 9*3+3=30 -> zwei Tests auf >=22 und >=11 nötig */
            if(pz1>=22)
               pz+=pz1-22;
            else if(pz1>=11)
               pz+=pz1-11;
            else
               pz+=pz1;

            pz += (kto[8]-'0') * 1 + 1;   /* Maximum von pz1 ist 9*1+1=10 -> kann direkt genommen werden */

            MOD_10_80;   /* pz%=10 */
#if DEBUG>0
            if(retvals)retvals->pz=pz;
#endif
            if(kto[9]-'0'==pz)return OK;
            pz+=5;
            MOD_10_10;
            CHECK_PZ10;
         }
         else{
#if DEBUG>0
      case 2119:
         if(retvals){
            retvals->methode="B9b";
            retvals->pz_methode=2119;
         }
#endif
            pz = (kto[3]-'0') * 6
               + (kto[4]-'0') * 5
               + (kto[5]-'0') * 4
               + (kto[6]-'0') * 3
               + (kto[7]-'0') * 2
               + (kto[8]-'0') * 1;

            MOD_11_176;   /* pz%=11 */

#if DEBUG>0
            if(retvals)retvals->pz=pz;
#endif
            if(kto[9]-'0'==pz)return OK;
            pz+=5;
            MOD_10_10;
            CHECK_PZ10;
         }



/* Berechnungsmethoden C0 bis C9 +§§§3
   Berechnung nach der Methode C0 +§§§4 */
/*
 * ######################################################################
 * #              Berechnung nach der Methode C0                        #
 * ######################################################################
 * #                                                                    #
 * # Die Kontonummer ist einschließlich der Prüfziffer 10-stellig,      #
 * # ggf. ist die Kontonummer für die Prüfzifferberechnung durch        #
 * # linksbündige Auffüllung mit Nullen 10-stellig darzustellen.        #
 * #                                                                    #
 * # Kontonummern mit zwei führenden Nullen sind nach Variante 1 zu     #
 * # prüfen. Führt die Berechnung nach der Variante 1 zu einem          #
 * # Prüfzifferfehler, ist die Berechnung nach Variante 2               #
 * # vorzunehmen.                                                       #
 * #                                                                    #
 * # Kontonummern mit weniger oder mehr als zwei führenden Nullen       #
 * # sind ausschließlich nach der Variante 2 zu                         #
 * # prüfen.                                                            #
 * #                                                                    #
 * # Variante 1:                                                        #
 * # Modulus 11, Gewichtung 2, 4, 8, 5, 10, 9, 7, 3, 6, 1, 2, 4         #
 * # Die Berechnung und mögliche Ergebnisse entsprechen                 #
 * # der Methode 52.                                                    #
 * #                                                                    #
 * # Variante 2:                                                        #
 * # Modulus 11, Gewichtung 2, 3, 4, 5, 6, 7, 8, 9, 3                   #
 * # Die Berechnung und mögliche Ergebnisse entsprechen der Methode 20  #
 * ######################################################################
 */
      case 120:
         if(*kto=='0' && kto[1]=='0' && kto[2]!='0'){
#if DEBUG>0
      case 1120:
         if(retvals){
            retvals->methode="C0a";
            retvals->pz_methode=1120;
         }
#endif
            if(!x_blz){
               ok=OK_TEST_BLZ_USED;
               x_blz="13051172";
            }
            else
               ok=OK;

               /* Generieren der Konto-Nr. des ESER-Altsystems */
            for(ptr=kto;*ptr=='0';ptr++);
            if(ptr>kto+2)return INVALID_KTO;
            kto_alt[0]=x_blz[4];
            kto_alt[1]=x_blz[5];
            kto_alt[2]=x_blz[6];
            kto_alt[3]=x_blz[7];
            kto_alt[4]= *ptr++;
            kto_alt[5]= *ptr++;
            while(*ptr=='0' && *ptr)ptr++;
            for(dptr=kto_alt+6;(*dptr= *ptr++);dptr++);
            p1=kto_alt[5];   /* Prüfziffer */
            kto_alt[5]='0';
            for(pz=0,ptr=dptr-1,i=0;ptr>=kto_alt;ptr--,i++)
               pz+=(*ptr-'0')*w52[i];
            kto_alt[5]=p1;
            pz=pz%11;
            p1=w52[i-6];

               /* passenden Faktor suchen */
            tmp=pz;
            for(i=0;i<10;i++){
               pz=tmp+p1*i;
               MOD_11_88;
               if(pz==10)break;
            }
            pz=i;
            INVALID_PZ10;
            if(*(kto_alt+5)-'0'==pz)return ok;
#if DEBUG>0
            if(untermethode)return FALSE;
#endif
         }
#if DEBUG>0
      case 2120:
         if(retvals){
            retvals->methode="C0b";
            retvals->pz_methode=2120;
         }
#endif
         pz = (kto[0]-'0') * 3
            + (kto[1]-'0') * 9
            + (kto[2]-'0') * 8
            + (kto[3]-'0') * 7
            + (kto[4]-'0') * 6
            + (kto[5]-'0') * 5
            + (kto[6]-'0') * 4
            + (kto[7]-'0') * 3
            + (kto[8]-'0') * 2;

         MOD_11_352;   /* pz%=11 */
         if(pz<=1)
            pz=0;
         else
            pz=11-pz;
         CHECK_PZ10;

/* Berechnung nach der Methode C1 +§§§4 */
/*
 * ######################################################################
 * #              Berechnung nach der Methode C1                        #
 * ######################################################################
 * #                                                                    #
 * # Die Kontonummer ist einschließlich der Prüfziffer 10-stellig,      #
 * # ggf. ist die Kontonummer für die Prüfzifferberechnung durch        #
 * # linksbündige Auffüllung mit Nullen 10-stellig darzustellen.        #
 * #                                                                    #
 * # Die Kontonummer ist einschließlich der Prüfziffer 10-stellig,      #
 * # ggf. ist die Kontonummer für die Prüfzifferberechnung durch        #
 * # linksbündige Auffüllung mit Nullen 10-stellig darzustellen.        #
 * #                                                                    #
 * # Kontonummern, die an der 1. Stelle der 10-stelligen                #
 * # Kontonummer einen Wert ungleich  5  beinhalten, sind nach der      #
 * # Variante 1 zu prüfen. Kontonummern, die an der 1. Stelle der       #
 * # 10-stelligen Kontonummer den Wert  5  beinhalten, sind nach        #
 * # der Variante 2 zu prüfen.                                          #
 * #                                                                    #
 * # Variante 1:                                                        #
 * # Modulus 11, Gewichtung 1, 2, 1, 2, 1, 2                            #
 * # Die Berechnung und mögliche Ergebnisse entsprechen der             #
 * # Methode 17. Führt die Berechnung nach der Variante 1 zu einem      #
 * # Prüfzifferfehler, so ist die Kontonummer falsch.                   #
 * #                                                                    #
 * # Variante 2:                                                        #
 * # Modulus 11, Gewichtung 1, 2, 1, 2, 1, 2                            #
 * # Die Kontonummer ist 10-stellig mit folgendem Aufbau:               #
 * #                                                                    #
 * # KNNNNNNNNP                                                         #
 * # K = Kontoartziffer                                                 #
 * # N = laufende Nummer                                                #
 * # P = Prüfziffer                                                     #
 * #                                                                    #
 * # Für die Berechnung fließen die Stellen 1 bis 9 ein. Stelle 10      #
 * # ist die ermittelte Prüfziffer. Die Stellen 1 bis 9 sind von        #
 * # links nach rechts mit den Ziffern 1, 2, 1, 2, 1, 2, 1, 2, 1        #
 * # zu multiplizieren. Die jeweiligen Produkte sind zu addieren,       #
 * # nachdem aus eventuell zweistelligen Produkten der 2., 4., 6.       #
 * # und 8. Stelle die Quersumme gebildet wurde. Von der Summe ist      #
 * # der Wert  1  zu subtrahieren. Das Ergebnis ist dann durch 11       #
 * # zu dividieren. Der verbleibende Rest wird von 10 subtrahiert.      #
 * # Das Ergebnis ist die Prüfziffer. Verbleibt nach der Division       #
 * # durch 11 kein Rest, ist die Prüfziffer 0.                          #
 * #                                                                    #
 * # Beispiel:                                                          #
 * #                                                                    #
 * # Stellen-Nr.: K   N   N   N   N   N   N   N   N   P                 #
 * # Konto-Nr.:   5   4   3   2   1   1   2   3   4   9                 #
 * # Gewichtung:  1   2   1   2   1   2   1   2   1                     #
 * #              5 + 8 + 3 + 4 + 1 + 2 + 2 + 6 + 4 = 35                #
 * # 35 - 1 = 34                                                        #
 * # 34 : 11 = 3, Rest 1                                                #
 * # 10 - 1 = 9 (Prüfziffer)                                            #
 * ######################################################################
 */

      case 121:
         if(*kto!='5'){ /* Prüfung nach Methode 17 */
#if DEBUG>0
      case 1121:
         if(retvals){
            retvals->methode="C1a";
            retvals->pz_methode=1121;
         }
#endif
#ifdef __ALPHA
            pz =  (kto[1]-'0')
               + ((kto[2]<'5') ? (kto[2]-'0')*2 : (kto[2]-'0')*2-9)
               +  (kto[3]-'0')
               + ((kto[4]<'5') ? (kto[4]-'0')*2 : (kto[4]-'0')*2-9)
               +  (kto[5]-'0')
               + ((kto[6]<'5') ? (kto[6]-'0')*2 : (kto[6]-'0')*2-9);
#else
            pz =(kto[1]-'0')+(kto[3]-'0')+(kto[5]-'0');
            if(kto[2]<'5')pz+=(kto[2]-'0')*2; else pz+=(kto[2]-'0')*2-9;
            if(kto[4]<'5')pz+=(kto[4]-'0')*2; else pz+=(kto[4]-'0')*2-9;
            if(kto[6]<'5')pz+=(kto[6]-'0')*2; else pz+=(kto[6]-'0')*2-9;
#endif

            pz-=1;
            MOD_11_44;   /* pz%=11 */
            if(pz)pz=10-pz;
            CHECK_PZ8;
         }
         else{
#if DEBUG>0
      case 2121:
         if(retvals){
            retvals->methode="C1b";
            retvals->pz_methode=2121;
         }
#endif
#ifdef __ALPHA
            pz =  (kto[0]-'0')
               + ((kto[1]<'5') ? (kto[1]-'0')*2 : (kto[1]-'0')*2-9)
               +  (kto[2]-'0')
               + ((kto[3]<'5') ? (kto[3]-'0')*2 : (kto[3]-'0')*2-9)
               +  (kto[4]-'0')
               + ((kto[5]<'5') ? (kto[5]-'0')*2 : (kto[5]-'0')*2-9)
               +  (kto[6]-'0')
               + ((kto[7]<'5') ? (kto[7]-'0')*2 : (kto[7]-'0')*2-9)
               +  (kto[8]-'0');
#else
            pz =(kto[0]-'0')+(kto[2]-'0')+(kto[4]-'0')+(kto[6]-'0')+(kto[8]-'0');
            if(kto[1]<'5')pz+=(kto[1]-'0')*2; else pz+=(kto[1]-'0')*2-9;
            if(kto[3]<'5')pz+=(kto[3]-'0')*2; else pz+=(kto[3]-'0')*2-9;
            if(kto[5]<'5')pz+=(kto[5]-'0')*2; else pz+=(kto[5]-'0')*2-9;
            if(kto[7]<'5')pz+=(kto[7]-'0')*2; else pz+=(kto[7]-'0')*2-9;
#endif

            pz-=1;
            MOD_11_44;   /* pz%=11 */
            if(pz)pz=10-pz;
            CHECK_PZ10;
         }


/* Berechnung nach der Methode C2 +§§§4 */
/*
 * ######################################################################
 * #              Berechnung nach der Methode C2                        #
 * ######################################################################
 * #                                                                    #
 * # Die Kontonummer ist einschließlich der Prüfziffer 10-stellig,      #
 * # ggf. ist die Kontonummer für die Prüfzifferberechnung durch        #
 * # linksbündige Auffüllung mit Nullen 10-stellig darzustellen.        #
 * # Die 10. Stelle der Kontonummer ist die Prüfziffer.                 #
 * #                                                                    #
 * # Variante 1:                                                        #
 * # Modulus 10, Gewichtung 3, 1, 3, 1, 3, 1, 3, 1, 3                   #
 * # Die Berechnung und mögliche Ergebnisse entsprechen der             #
 * # Methode 22. Führt die Berechnung nach Variante 1 zu einem          #
 * # Prüfzifferfehler, so ist nach Variante 2 zu prüfen.                #
 * #                                                                    #
 * # Variante 2:                                                        #
 * # Modulus 10, Gewichtung 2, 1, 2, 1, 2, 1, 2, 1, 2                   #
 * # Die Berechnung und mögliche Ergebnisse entsprechen der             #
 * # Methode 00.                                                        #
 * ######################################################################
 */

      case 122:
#if DEBUG>0
      case 1122:
         if(retvals){
            retvals->methode="C2a";
            retvals->pz_methode=1122;
         }
#endif
         pz = (kto[1]-'0')+(kto[3]-'0')+(kto[5]-'0')+(kto[7]-'0');

            if(kto[0]<'4')
               pz+=(kto[0]-'0')*3;
            else if(kto[0]<'7')
               pz+=(kto[0]-'0')*3-10;
            else
               pz+=(kto[0]-'0')*3-20;

            if(kto[2]<'4')
               pz+=(kto[2]-'0')*3;
            else if(kto[2]<'7')
               pz+=(kto[2]-'0')*3-10;
            else
               pz+=(kto[2]-'0')*3-20;

            if(kto[4]<'4')
               pz+=(kto[4]-'0')*3;
            else if(kto[4]<'7')
               pz+=(kto[4]-'0')*3-10;
            else
               pz+=(kto[4]-'0')*3-20;

            if(kto[6]<'4')
               pz+=(kto[6]-'0')*3;
            else if(kto[6]<'7')
               pz+=(kto[6]-'0')*3-10;
            else
               pz+=(kto[6]-'0')*3-20;

            if(kto[8]<'4')
               pz+=(kto[8]-'0')*3;
            else if(kto[8]<'7')

               pz+=(kto[8]-'0')*3-10;
            else
               pz+=(kto[8]-'0')*3-20;

         MOD_10_80;   /* pz%=10 */
         if(pz)pz=10-pz;
         CHECK_PZX10;

#if DEBUG>0
      case 2122:
         if(retvals){
            retvals->methode="C2b";
            retvals->pz_methode=2122;
         }
#endif
#ifdef __ALPHA
         pz = ((kto[0]<'5') ? (kto[0]-'0')*2 : (kto[0]-'0')*2-9)
            +  (kto[1]-'0')
            + ((kto[2]<'5') ? (kto[2]-'0')*2 : (kto[2]-'0')*2-9)
            +  (kto[3]-'0')
            + ((kto[4]<'5') ? (kto[4]-'0')*2 : (kto[4]-'0')*2-9)
            +  (kto[5]-'0')
            + ((kto[6]<'5') ? (kto[6]-'0')*2 : (kto[6]-'0')*2-9)
            +  (kto[7]-'0')
            + ((kto[8]<'5') ? (kto[8]-'0')*2 : (kto[8]-'0')*2-9);
#else
         pz=(kto[1]-'0')+(kto[3]-'0')+(kto[5]-'0')+(kto[7]-'0');
         if(kto[0]<'5')pz+=(kto[0]-'0')*2; else pz+=(kto[0]-'0')*2-9;
         if(kto[2]<'5')pz+=(kto[2]-'0')*2; else pz+=(kto[2]-'0')*2-9;
         if(kto[4]<'5')pz+=(kto[4]-'0')*2; else pz+=(kto[4]-'0')*2-9;
         if(kto[6]<'5')pz+=(kto[6]-'0')*2; else pz+=(kto[6]-'0')*2-9;
         if(kto[8]<'5')pz+=(kto[8]-'0')*2; else pz+=(kto[8]-'0')*2-9;
#endif
         MOD_10_80;   /* pz%=10 */
         if(pz)pz=10-pz;
         CHECK_PZ10;


/* Berechnung nach der Methode C3 +§§§4 */
/*
 * ######################################################################
 * #              Berechnung nach der Methode C3                        #
 * ######################################################################
 * #                                                                    #
 * # Die Kontonummer ist einschließlich der Prüfziffer 10-stellig,      #
 * # ggf. ist die Kontonummer für die Prüfzifferberechnung durch        #
 * # linksbündige Auffüllung mit Nullen 10-stellig darzustellen.        #
 * # Die 10. Stelle der Kontonummer ist die Prüfziffer. Kontonummern,   #
 * # die an der 1. Stelle der 10-stelligen Kontonummer einen Wert       #
 * # ungleich 9 beinhalten, sind nach der Variante 1 zu prüfen.         #
 * # Kontonummern, die an der 1. Stelle der 10-stelligen Kontonummer    #
 * # den Wert 9 beinhalten, sind nach der Variante 2 zu prüfen.         #
 * #                                                                    #
 * # Variante 1:                                                        #
 * # Modulus 10, Gewichtung 2, 1, 2, 1, 2, 1, 2, 1, 2                   #
 * # Die Berechnung und mögliche Ergebnisse entsprechen der             #
 * # Methode 00.                                                        #
 * #                                                                    #
 * # Variante 2:                                                        #
 * # Modulus 11, Gewichtung 2, 3, 4, 5, 6, 0, 0, 0, 0                   #
 * # Die Berechnung und mögliche Ergebnisse entsprechen der             #
 * # Methode 58.                                                        #
 * ######################################################################
 */

      case 123:
         if(kto[0]!='9'){
#if DEBUG>0
      case 1123:
         if(retvals){
            retvals->methode="C3a";
            retvals->pz_methode=1123;
         }
#endif
#ifdef __ALPHA
            pz = ((kto[0]<'5') ? (kto[0]-'0')*2 : (kto[0]-'0')*2-9)
               +  (kto[1]-'0')
               + ((kto[2]<'5') ? (kto[2]-'0')*2 : (kto[2]-'0')*2-9)
               +  (kto[3]-'0')
               + ((kto[4]<'5') ? (kto[4]-'0')*2 : (kto[4]-'0')*2-9)
               +  (kto[5]-'0')
               + ((kto[6]<'5') ? (kto[6]-'0')*2 : (kto[6]-'0')*2-9)
               +  (kto[7]-'0')
               + ((kto[8]<'5') ? (kto[8]-'0')*2 : (kto[8]-'0')*2-9);
#else
            pz=(kto[1]-'0')+(kto[3]-'0')+(kto[5]-'0')+(kto[7]-'0');
            if(kto[0]<'5')pz+=(kto[0]-'0')*2; else pz+=(kto[0]-'0')*2-9;
            if(kto[2]<'5')pz+=(kto[2]-'0')*2; else pz+=(kto[2]-'0')*2-9;
            if(kto[4]<'5')pz+=(kto[4]-'0')*2; else pz+=(kto[4]-'0')*2-9;
            if(kto[6]<'5')pz+=(kto[6]-'0')*2; else pz+=(kto[6]-'0')*2-9;
            if(kto[8]<'5')pz+=(kto[8]-'0')*2; else pz+=(kto[8]-'0')*2-9;
#endif
            MOD_10_80;   /* pz%=10 */
            if(pz)pz=10-pz;
            CHECK_PZ10;
         }
         else{
#if DEBUG>0
      case 2123:
         if(retvals){
            retvals->methode="C3b";
            retvals->pz_methode=2123;
         }
#endif
            pz = (kto[4]-'0') * 6
               + (kto[5]-'0') * 5
               + (kto[6]-'0') * 4
               + (kto[7]-'0') * 3
               + (kto[8]-'0') * 2;

            MOD_11_176;   /* pz%=11 */
            if(pz)pz=11-pz;
            INVALID_PZ10;
            CHECK_PZ10;
         }

/* Berechnung nach der Methode C4 +§§§4 */
/*
 * ######################################################################
 * #              Berechnung nach der Methode C4                        #
 * ######################################################################
 * #                                                                    #
 * # Die Kontonummer ist einschließlich der Prüfziffer 10-stellig,      #
 * # ggf. ist die Kontonummer für die Prüfzifferberechnung durch        #
 * # linksbündige Auffüllung mit Nullen 10-stellig darzustellen. Die    #
 * # 10. Stelle der Kontonummer ist die Prüfziffer.                     #
 * # Kontonummern, die an der 1. Stelle der 10-stelligen Kontonummer    #
 * # einen Wert ungleich 9 beinhalten, sind nach der Variante 1 zu      #
 * # prüfen.                                                            #
 * # Kontonummern, die an der 1. Stelle der 10-stelligen Kontonummer    #
 * # den Wert 9 beinhalten, sind nach der Variante 2 zu prüfen.         #
 * #                                                                    #
 * # Variante 1:                                                        #
 * # Modulus 11, Gewichtung 2, 3, 4, 5                                  #
 * # Die Berechnung und mögliche Ergebnisse entsprechen der Methode 15. #
 * #                                                                    #
 * # Variante 2:                                                        #
 * # Modulus 11, Gewichtung 2, 3, 4, 5, 6, 0, 0, 0, 0                   #
 * # Die Berechnung und mögliche Ergebnisse entsprechen der Methode 58. #
 * ######################################################################
 */

      case 124:
         if(kto[0]!='9'){
#if DEBUG>0
      case 1124:
         if(retvals){
            retvals->methode="C4a";
            retvals->pz_methode=1124;
         }
#endif
            pz = (kto[5]-'0') * 5
               + (kto[6]-'0') * 4
               + (kto[7]-'0') * 3
               + (kto[8]-'0') * 2;

            MOD_11_88;    /* pz%=11 */
            if(pz<=1)
               pz=0;
            else
               pz=11-pz;
            CHECK_PZ10;
         }
         else{
#if DEBUG>0
      case 2124:
         if(retvals){
            retvals->methode="C4b";
            retvals->pz_methode=2124;
         }
#endif
            pz = (kto[4]-'0') * 6
               + (kto[5]-'0') * 5
               + (kto[6]-'0') * 4
               + (kto[7]-'0') * 3
               + (kto[8]-'0') * 2;

            MOD_11_176;   /* pz%=11 */
            if(pz)pz=11-pz;
            INVALID_PZ10;
            CHECK_PZ10;
         }

/* Berechnung nach der Methode C5 +§§§4 */
/*
 * ######################################################################
 * #              Berechnung nach der Methode C5                        #
 * ######################################################################
 * #                                                                    #
 * # Die Kontonummern sind einschließlich der Prüfziffer 6- oder 8-     #
 * # bis 10-stellig, ggf. ist die Kontonummer für die Prüfziffer-       #
 * # berechnung durch linksbündige Auffüllung mit Nullen 10-stellig     #
 * # darzustellen.                                                      #
 * #                                                                    #
 * # Die Berechnung der Prüfziffer und die möglichen Ergebnisse         #
 * # richten sich nach dem jeweils bei der entsprechenden Variante      #
 * # angegebenen Kontonummernkreis. Entspricht eine Kontonummer         #
 * # keinem der vorgegebenen Kontonummernkreise oder führt die          #
 * # Berechnung der Prüfziffer nach der vorgegebenen Variante zu        #
 * # einem Prüfzifferfehler, so ist die Kontonummer ungültig.           #
 * #                                                                    #
 * # Variante 1:                                                        #
 * # Modulus 10, Gewichtung 2, 1, 2, 1, 2                               #
 * # Die Berechnung und mögliche Ergebnisse entsprechen der             #
 * # Methode 75.                                                        #
 * #                                                                    #
 * # 6-stellige Kontonummern; 5. Stelle = 1-8                           #
 * # Kontonummernkreis 0000100000 bis 0000899999                        #
 * #                                                                    #
 * # 9-stellige Kontonummern; 2. Stelle = 1-8                           #
 * # Kontonummernkreis 0100000000 bis 0899999999                        #
 * #                                                                    #
 * # Variante 2:                                                        #
 * # Modulus 10, iterierte Transformation                               #
 * # Die Berechnung und mögliche Ergebnisse entsprechen der             #
 * # Methode 29.                                                        #
 * #                                                                    #
 * # 10-stellige Kontonummern, 1. Stelle = 1, 4, 5, 6 oder 9            #
 * # Kontonummernkreis 1000000000 bis 1999999999                        #
 * # Kontonummernkreis 4000000000 bis 6999999999                        #
 * # Kontonummernkreis 9000000000 bis 9999999999                        #
 * #                                                                    #
 * # Variante 3:                                                        #
 * # Modulus 10, Gewichtung 2, 1, 2, 1, 2, 1, 2, 1, 2                   #
 * # Die Berechnung und mögliche Ergebnisse entsprechen der             #
 * # Methode 00.                                                        #
 * # 10-stellige Kontonummern, 1. Stelle = 3                            #
 * # Kontonummernkreis 3000000000 bis 3999999999                        #
 * #                                                                    #
 * # Variante 4:                                                        #
 * # Für die folgenden Kontonummernkreise gilt die Methode 09           #
 * # (keine Prüfzifferberechnung).                                      #
 * #                                                                    #
 * # 8-stellige Kontonummern; 3. Stelle = 3, 4 oder 5                   #
 * # Kontonummernkreis 0030000000 bis 0059999999                        #
 * #                                                                    #
 * # 10-stellige Kontonummern; 1.+ 2. Stelle = 70 oder 85               #
 * # Kontonummernkreis 7000000000 bis 7099999999                        #
 * # Kontonummernkreis 8500000000 bis 8599999999                        #
 * ######################################################################
 */

      case 125:
#if DEBUG>0
      case 1125:
         if(retvals){
            retvals->methode="C5a";
            retvals->pz_methode=1125;
         }
#endif

            /* Variante 1a:
             *  6-stellige Kontonummern; 5. Stelle = 1-8, Prüfziffer an Stelle 10
             */
         if(kto[0]=='0' && kto[1]=='0' && kto[2]=='0' && kto[3]=='0' && kto[4]>='1' && kto[4]<='8'){
#ifdef __ALPHA
            pz = ((kto[4]<'5') ? (kto[4]-'0')*2 : (kto[4]-'0')*2-9)
               +  (kto[5]-'0')
               + ((kto[6]<'5') ? (kto[6]-'0')*2 : (kto[6]-'0')*2-9)
               +  (kto[7]-'0')
               + ((kto[8]<'5') ? (kto[8]-'0')*2 : (kto[8]-'0')*2-9);
#else
            pz=(kto[5]-'0')+(kto[7]-'0');
            if(kto[4]<'5')pz+=(kto[4]-'0')*2; else pz+=(kto[4]-'0')*2-9;
            if(kto[6]<'5')pz+=(kto[6]-'0')*2; else pz+=(kto[6]-'0')*2-9;
            if(kto[8]<'5')pz+=(kto[8]-'0')*2; else pz+=(kto[8]-'0')*2-9;
#endif
         MOD_10_80;   /* pz%=10 */
            if(pz)pz=10-pz;
            CHECK_PZ10;
         }

            /* Variante 1b:
             *    9-stellige Kontonummern; 2. Stelle = 1-8, Prüfziffer an Stelle 7
             */
         else if(kto[0]=='0' && kto[1]>='1' && kto[1]<='8'){
#ifdef __ALPHA
            pz = ((kto[1]<'5') ? (kto[1]-'0')*2 : (kto[1]-'0')*2-9)
               +  (kto[2]-'0')
               + ((kto[3]<'5') ? (kto[3]-'0')*2 : (kto[3]-'0')*2-9)
               +  (kto[4]-'0')
               + ((kto[5]<'5') ? (kto[5]-'0')*2 : (kto[5]-'0')*2-9);
#else
            pz=(kto[2]-'0')+(kto[4]-'0');
            if(kto[1]<'5')pz+=(kto[1]-'0')*2; else pz+=(kto[1]-'0')*2-9;
            if(kto[3]<'5')pz+=(kto[3]-'0')*2; else pz+=(kto[3]-'0')*2-9;
            if(kto[5]<'5')pz+=(kto[5]-'0')*2; else pz+=(kto[5]-'0')*2-9;
#endif
         MOD_10_40;   /* pz%=10 */
            if(pz)pz=10-pz;
            CHECK_PZ7;
         }

            /* Variante 2: 10-stellige Kontonummern, 1. Stelle = 1, 4, 5, 6 oder 9 */
         else if(kto[0]=='1' || (kto[0]>='4' && kto[0]<='6') || kto[0]=='9'){
#if DEBUG>0
      case 2125:
         if(retvals){
            retvals->methode="C5b";
            retvals->pz_methode=2125;
         }
#endif
            pz = m10h_digits[0][(unsigned int)(kto[0]-'0')]
               + m10h_digits[3][(unsigned int)(kto[1]-'0')]
               + m10h_digits[2][(unsigned int)(kto[2]-'0')]
               + m10h_digits[1][(unsigned int)(kto[3]-'0')]
               + m10h_digits[0][(unsigned int)(kto[4]-'0')]
               + m10h_digits[3][(unsigned int)(kto[5]-'0')]
               + m10h_digits[2][(unsigned int)(kto[6]-'0')]
               + m10h_digits[1][(unsigned int)(kto[7]-'0')]
               + m10h_digits[0][(unsigned int)(kto[8]-'0')];
            MOD_10_80;   /* pz%=10 */
            if(pz)pz=10-pz;
            CHECK_PZ10;
         }

            /* Variante 3: 10-stellige Kontonummern, 1. Stelle = 3 */
         else if(kto[0]=='3'){
#if DEBUG>0
      case 3125:
         if(retvals){
            retvals->methode="C5c";
            retvals->pz_methode=3125;
         }
#endif
#ifdef __ALPHA
            pz = ((kto[0]<'5') ? (kto[0]-'0')*2 : (kto[0]-'0')*2-9)
               +  (kto[1]-'0')
               + ((kto[2]<'5') ? (kto[2]-'0')*2 : (kto[2]-'0')*2-9)
               +  (kto[3]-'0')
               + ((kto[4]<'5') ? (kto[4]-'0')*2 : (kto[4]-'0')*2-9)
               +  (kto[5]-'0')
               + ((kto[6]<'5') ? (kto[6]-'0')*2 : (kto[6]-'0')*2-9)
               +  (kto[7]-'0')
               + ((kto[8]<'5') ? (kto[8]-'0')*2 : (kto[8]-'0')*2-9);
#else
            pz=(kto[1]-'0')+(kto[3]-'0')+(kto[5]-'0')+(kto[7]-'0');
            if(kto[0]<'5')pz+=(kto[0]-'0')*2; else pz+=(kto[0]-'0')*2-9;
            if(kto[2]<'5')pz+=(kto[2]-'0')*2; else pz+=(kto[2]-'0')*2-9;
            if(kto[4]<'5')pz+=(kto[4]-'0')*2; else pz+=(kto[4]-'0')*2-9;
            if(kto[6]<'5')pz+=(kto[6]-'0')*2; else pz+=(kto[6]-'0')*2-9;
            if(kto[8]<'5')pz+=(kto[8]-'0')*2; else pz+=(kto[8]-'0')*2-9;
#endif
            MOD_10_80;   /* pz%=10 */
            if(pz)pz=10-pz;
            CHECK_PZ10;
         }

            /* Variante 4:
             *    8-stellige KOntonummern mit 3. Stelle = 3,4, oder 5
             *    10-stellige Kontonummern, 1. und 2. Stelle = 70 oder 85:
             */
         else if( (kto[0]=='0' && kto[1]=='0' && kto[2]>='3' && kto[2]<='5')
               || (kto[0]=='7' && kto[1]=='0')
               || (kto[0]=='8' && kto[1]=='5')){
#if DEBUG>0
      case 4125:
         if(retvals){
            retvals->methode="C5d";
            retvals->pz_methode=4125;
         }
#endif
         return OK_NO_CHK;
      }
      else  /* Kontonummer entspricht keinem vorgegebenen Kontenkreis */
         return INVALID_KTO;

/* Berechnung nach der Methode C6 +§§§4 */
/*
 * ######################################################################
 * #   Berechnung nach der Methode C6  (geändert zum 9. März 2009)      #
 * ######################################################################
 * # Modulus 10, Gewichtung 1, 2, 1, 2, 1, 2, 1, 2                      #
 * #                                                                    #
 * # Die Kontonummer ist 10-stellig, ggf. ist die Kontonummer für die   #
 * # Prüfzifferberechnung durch linksbündige Auffüllung mit Nullen      #
 * # 10-stellig darzustellen. Die 10. Stelle der Konto-nummer ist die   #
 * # Prüfziffer.                                                        #
 * #                                                                    #
 * # Kontonummern, die an der 1. Stelle von links der 10-stelligen      #
 * # Kontonummer einen der Wert 3, 4, 5, 6 oder 8 beinhalten sind       #
 * # falsch.                                                            #
 * #                                                                    #
 * # Kontonummern, die an der 1. Stelle von links der 10-stelligen      #
 * # Kontonummer einen der Werte 0, 1, 2, 7 oder 9 beinhalten sind wie  #
 * # folgt zu prüfen:                                                   #
 * #                                                                    #
 * # Für die Berechnung der Prüfziffer werden die Stellen 2 bis 9 der   #
 * # Kontonummer verwendet. Diese Stellen sind links um eine Zahl       #
 * # (Konstante) gemäß der folgenden Tabelle zu ergänzen.               #
 * #                                                                    #
 * #   1. Stelle von links                                              #
 * #     der 10-stelligen                                               #
 * #       Kontonummer    Zahl (Konstante)                              #
 * #                                                                    #
 * #            0          4451970                                      #
 * #            1          4451981                                      #
 * #            2          4451992                                      #
 * #            7          5499570                                      #
 * #            9          5499579                                      #
 * #                                                                    #
 * # Die Berechnung und mögliche Ergebnisse entsprechen der Methode 00. #
 * ######################################################################
 * #                                                                    #
 * # Anmerkung zur Berechnung (MP): Da die Konstante immer nur einen    #
 * # festen Wert zur Berechnung beiträgt, wird diese Berechnung nicht   #
 * # gemacht, sondern gleich der Wert als Initialwert für die Quersumme #
 * # verwendet. Die Berechnung beginnt erst mit der zweiten Stelle der  #
 * # Kontonummer.                                                       #
 * ######################################################################
 */

      case 126:
#if DEBUG>0
         if(retvals){
            retvals->methode="C6";
            retvals->pz_methode=126;
         }
#endif
            /* neue Berechnungsmethode für C6, gültig ab 9.3.2009 */
         switch(kto[0]){
            case '0': pz= 30; break;
            case '1': pz= 33; break;
            case '2': pz= 36; break;
            case '7': pz= 31; break;
            case '9': pz= 40; break;
            default: return INVALID_KTO;
         }

#ifdef __ALPHA
         pz += (kto[1]-'0')
            + ((kto[2]<'5') ? (kto[2]-'0')*2 : (kto[2]-'0')*2-9)
            +  (kto[3]-'0')
            + ((kto[4]<'5') ? (kto[4]-'0')*2 : (kto[4]-'0')*2-9)
            +  (kto[5]-'0')
            + ((kto[6]<'5') ? (kto[6]-'0')*2 : (kto[6]-'0')*2-9)
            +  (kto[7]-'0')
            + ((kto[8]<'5') ? (kto[8]-'0')*2 : (kto[8]-'0')*2-9);
#else
         pz+=(kto[1]-'0')+(kto[3]-'0')+(kto[5]-'0')+(kto[7]-'0');
         if(kto[2]<'5')pz+=(kto[2]-'0')*2; else pz+=(kto[2]-'0')*2-9;
         if(kto[4]<'5')pz+=(kto[4]-'0')*2; else pz+=(kto[4]-'0')*2-9;
         if(kto[6]<'5')pz+=(kto[6]-'0')*2; else pz+=(kto[6]-'0')*2-9;
         if(kto[8]<'5')pz+=(kto[8]-'0')*2; else pz+=(kto[8]-'0')*2-9;
#endif
         MOD_10_160;   /* pz%=10 */
         if(pz)pz=10-pz;
         CHECK_PZ10;

/* Berechnung nach der Methode C7 +§§§4 */
/*
 * ######################################################################
 * #              Berechnung nach der Methode C7                        #
 * ######################################################################
 * # Die Kontonummer ist einschließlich der Prüfziffer 10-stellig,      #
 * # ggf. ist die Kontonummer für die Prüfzifferberechnung durch        #
 * # linksbündige Auffüllung mit Nullen 10-stellig darzustellen.        #
 * #                                                                    #
 * # Variante 1                                                         #
 * # Modulus 10, Gewichtung 2, 1, 2, 1, 2, 1                            #
 * # Die Berechnung und mögliche Ergebnisse entsprechen der             #
 * # Methode 63. Führt die Berechnung nach Variante 1 zu einem          #
 * # Prüfzifferfehler, so ist nach Variante 2 zu prüfen.                #
 * #                                                                    #
 * # Variante 2:                                                        #
 * # Modulus 11, Gewichtung 2, 3, 4, 5, 6, 7 (modifiziert)              #
 * # Die Berechnung und mögliche Ergebnisse entsprechen der Methode 06  #
 * ######################################################################
 */

      case 127:
         if(*kto=='0'){ /* bei Methode 63 sind 10-stellige Kontonummern falsch */
            if(*(kto+1)=='0' && *(kto+2)=='0'){
               /* evl. Unterkonto weggelassen; das kommt wohl eher vor als
                * 7-stellige Nummern (Hinweis T.F.); stimmt auch mit
                * http://www.kontonummern.de/check.php überein.
               */
               #if DEBUG>0
      case 3127:
         if(retvals){
            retvals->methode="C7c";
            retvals->pz_methode=3127;
         }
#endif
#ifdef __ALPHA
               pz =   (kto[3]-'0')
                  +  ((kto[4]<'5') ? (kto[4]-'0')*2 : (kto[4]-'0')*2-9)
                  +   (kto[5]-'0')
                  +  ((kto[6]<'5') ? (kto[6]-'0')*2 : (kto[6]-'0')*2-9)
                  +   (kto[7]-'0')
                  +  ((kto[8]<'5') ? (kto[8]-'0')*2 : (kto[8]-'0')*2-9);
#else
               pz=(kto[3]-'0')+(kto[5]-'0')+(kto[7]-'0');
               if(kto[4]<'5')pz+=(kto[4]-'0')*2; else pz+=(kto[4]-'0')*2-9;
               if(kto[6]<'5')pz+=(kto[6]-'0')*2; else pz+=(kto[6]-'0')*2-9;
               if(kto[8]<'5')pz+=(kto[8]-'0')*2; else pz+=(kto[8]-'0')*2-9;
#endif
               MOD_10_80;   /* pz%=10 */
               if(pz)pz=10-pz;
               CHECK_PZX10;
            }
            else{
               #if DEBUG>0
      case 1127:
         if(retvals){
            retvals->methode="C7a";
            retvals->pz_methode=1127;
         }
#endif
#ifdef __ALPHA
                  pz =   (kto[1]-'0')
                  +  ((kto[2]<'5') ? (kto[2]-'0')*2 : (kto[2]-'0')*2-9)
                  +   (kto[3]-'0')
                  +  ((kto[4]<'5') ? (kto[4]-'0')*2 : (kto[4]-'0')*2-9)
                  +   (kto[5]-'0')
                  +  ((kto[6]<'5') ? (kto[6]-'0')*2 : (kto[6]-'0')*2-9);
#else
               pz=(kto[1]-'0')+(kto[3]-'0')+(kto[5]-'0');
               if(kto[2]<'5')pz+=(kto[2]-'0')*2; else pz+=(kto[2]-'0')*2-9;
               if(kto[4]<'5')pz+=(kto[4]-'0')*2; else pz+=(kto[4]-'0')*2-9;
               if(kto[6]<'5')pz+=(kto[6]-'0')*2; else pz+=(kto[6]-'0')*2-9;
#endif
               MOD_10_80;   /* pz%=10 */
               if(pz)pz=10-pz;
               CHECK_PZX8;
            }
         }

      /* Variante 2 */
#if DEBUG>0
      case 2127:
         if(retvals){
            retvals->methode="C7b";
            retvals->pz_methode=2127;
         }
#endif
         pz = (kto[0]-'0') * 4
            + (kto[1]-'0') * 3
            + (kto[2]-'0') * 2
            + (kto[3]-'0') * 7
            + (kto[4]-'0') * 6
            + (kto[5]-'0') * 5
            + (kto[6]-'0') * 4
            + (kto[7]-'0') * 3
            + (kto[8]-'0') * 2;

         MOD_11_176;   /* pz%=11 */
         if(pz<=1)
            pz=0;
         else
            pz=11-pz;
         CHECK_PZ10;


/* Berechnung nach der Methode C8 +§§§4 */
/*
 * ######################################################################
 * #              Berechnung nach der Methode C8                        #
 * ######################################################################
 * # Die Kontonummer ist einschließlich der Prüfziffer 10-stellig,      #
 * # ggf. ist die Kontonummer für die Prüfzifferberechnung durch        #
 * # linksbündige Auffüllung mit Nullen 10-stellig darzustellen.        #
 * #                                                                    #
 * # Variante 1:                                                        #
 * # Modulus 10, Gewichtung 2, 1, 2, 1, 2, 1, 2, 1, 2                   #  
 * # Gewichtung und Berechnung erfolgen nach der Methode 00. Führt      # 
 * # die Berechnung nach Variante 1 zu einem Prüfzifferfehler, so ist   #    
 * # nach Variante 2 zu prüfen.                                         #
 * #                                                                    #
 * # Variante 2:                                                        #
 * # Modulus 11, Gewichtung 2, 3, 4, 5, 6, 7, 2, 3, 4                   #  
 * # Gewichtung und Berechnung erfolgen nach der Methode 04. Führt      # 
 * # auch die Berechnung nach Variante 2 zu einem Prüfzifferfehler,     #  
 * # oder ist keine gültige Prüfziffer zu ermitteln, d.h. Rest 1 nach   #
 * # der Division durch 11, so ist nach Variante 3 zu prüfen.           #
 * #                                                                    #
 * # Variante 3:                                                        #
 * # Modulus 11, Gewichtung 2, 3, 4, 5, 6, 7, 8, 9, 10                  #   
 * # Gewichtung und Berechnung erfolgen nach der Methode 07.            #  
 * ######################################################################
 */

      case 128:
      /* Variante 1 */
#if DEBUG>0
      case 1128:
         if(retvals){
            retvals->methode="C8a";
            retvals->pz_methode=1128;
         }
#endif
#ifdef __ALPHA
         pz = ((kto[0]<'5') ? (kto[0]-'0')*2 : (kto[0]-'0')*2-9)
            +  (kto[1]-'0')
            + ((kto[2]<'5') ? (kto[2]-'0')*2 : (kto[2]-'0')*2-9)
            +  (kto[3]-'0')
            + ((kto[4]<'5') ? (kto[4]-'0')*2 : (kto[4]-'0')*2-9)
            +  (kto[5]-'0')
            + ((kto[6]<'5') ? (kto[6]-'0')*2 : (kto[6]-'0')*2-9)
            +  (kto[7]-'0')
            + ((kto[8]<'5') ? (kto[8]-'0')*2 : (kto[8]-'0')*2-9);
#else
         pz=(kto[1]-'0')+(kto[3]-'0')+(kto[5]-'0')+(kto[7]-'0');
         if(kto[0]<'5')pz+=(kto[0]-'0')*2; else pz+=(kto[0]-'0')*2-9;
         if(kto[2]<'5')pz+=(kto[2]-'0')*2; else pz+=(kto[2]-'0')*2-9;
         if(kto[4]<'5')pz+=(kto[4]-'0')*2; else pz+=(kto[4]-'0')*2-9;
         if(kto[6]<'5')pz+=(kto[6]-'0')*2; else pz+=(kto[6]-'0')*2-9;
         if(kto[8]<'5')pz+=(kto[8]-'0')*2; else pz+=(kto[8]-'0')*2-9;
#endif
         MOD_10_80;   /* pz%=10 */
         if(pz)pz=10-pz;
         CHECK_PZX10;

      /* Variante 2 */
#if DEBUG>0
      case 2128:
         if(retvals){
            retvals->methode="C8b";
            retvals->pz_methode=2128;
         }
#endif
         pz = (kto[0]-'0') * 4
            + (kto[1]-'0') * 3
            + (kto[2]-'0') * 2
            + (kto[3]-'0') * 7
            + (kto[4]-'0') * 6
            + (kto[5]-'0') * 5
            + (kto[6]-'0') * 4
            + (kto[7]-'0') * 3
            + (kto[8]-'0') * 2;

         MOD_11_176;   /* pz%=11 */
         if(pz)pz=11-pz;
         CHECK_PZX10;

      /* Variante 3 */
#if DEBUG>0
      case 3128:
         if(retvals){
            retvals->methode="C8c";
            retvals->pz_methode=3128;
         }
#endif
         pz = (kto[0]-'0') * 10
            + (kto[1]-'0') * 9
            + (kto[2]-'0') * 8
            + (kto[3]-'0') * 7
            + (kto[4]-'0') * 6
            + (kto[5]-'0') * 5
            + (kto[6]-'0') * 4
            + (kto[7]-'0') * 3
            + (kto[8]-'0') * 2;

         MOD_11_352;   /* pz%=11 */
         if(pz)pz=11-pz;
         INVALID_PZ10;
         CHECK_PZ10;

/* Berechnung nach der Methode C9 +§§§4 */
/*
 * ######################################################################
 * #              Berechnung nach der Methode C9                        #
 * ######################################################################
 * # Die Kontonummer ist einschließlich der Prüfziffer 10-stellig,      #
 * # ggf. ist die Kontonummer für die Prüfzifferberechnung durch        #
 * # linksbündige Auffüllung mit Nullen 10-stellig darzustellen.        #
 * #                                                                    #
 * # Variante 1:                                                        #
 * # Modulus 10, Gewichtung 2, 1, 2, 1, 2, 1, 2, 1, 2                   #
 * # Gewichtung und Berechnung erfolgen nach der Methode 00. Führt die  #
 * # Berechnung nach Variante 1 zu einem Prüfzifferfehler, so ist nach  #
 * # Variante 2 zu prüfen.                                              #
 * #                                                                    #
 * # Variante 2:                                                        #
 * # Modulus 11, Gewichtung 2, 3, 4, 5, 6, 7, 8, 9, 10                  #
 * # Gewichtung und Berechnung erfolgen nach der Methode 07.            #
 * ######################################################################
 */

      case 129:
      /* Variante 1 */
#if DEBUG>0
      case 1129:
         if(retvals){
            retvals->methode="C9a";
            retvals->pz_methode=1129;
         }
#endif
#ifdef __ALPHA
         pz = ((kto[0]<'5') ? (kto[0]-'0')*2 : (kto[0]-'0')*2-9)
            +  (kto[1]-'0')
            + ((kto[2]<'5') ? (kto[2]-'0')*2 : (kto[2]-'0')*2-9)
            +  (kto[3]-'0')
            + ((kto[4]<'5') ? (kto[4]-'0')*2 : (kto[4]-'0')*2-9)
            +  (kto[5]-'0')
            + ((kto[6]<'5') ? (kto[6]-'0')*2 : (kto[6]-'0')*2-9)
            +  (kto[7]-'0')
            + ((kto[8]<'5') ? (kto[8]-'0')*2 : (kto[8]-'0')*2-9);
#else
         pz=(kto[1]-'0')+(kto[3]-'0')+(kto[5]-'0')+(kto[7]-'0');
         if(kto[0]<'5')pz+=(kto[0]-'0')*2; else pz+=(kto[0]-'0')*2-9;
         if(kto[2]<'5')pz+=(kto[2]-'0')*2; else pz+=(kto[2]-'0')*2-9;
         if(kto[4]<'5')pz+=(kto[4]-'0')*2; else pz+=(kto[4]-'0')*2-9;
         if(kto[6]<'5')pz+=(kto[6]-'0')*2; else pz+=(kto[6]-'0')*2-9;
         if(kto[8]<'5')pz+=(kto[8]-'0')*2; else pz+=(kto[8]-'0')*2-9;
#endif
         MOD_10_80;   /* pz%=10 */
         if(pz)pz=10-pz;
         CHECK_PZX10;

      /* Variante 2 */
#if DEBUG>0
      case 2129:
         if(retvals){
            retvals->methode="C9b";
            retvals->pz_methode=2129;
         }
#endif
         pz = (kto[0]-'0') * 10
            + (kto[1]-'0') * 9
            + (kto[2]-'0') * 8
            + (kto[3]-'0') * 7
            + (kto[4]-'0') * 6
            + (kto[5]-'0') * 5
            + (kto[6]-'0') * 4
            + (kto[7]-'0') * 3
            + (kto[8]-'0') * 2;

         MOD_11_352;   /* pz%=11 */
         if(pz)pz=11-pz;
         INVALID_PZ10;
         CHECK_PZ10;

/* Berechnungsmethoden D0 bis D3 +§§§3
 * Berechnung nach der Methode D0 +§§§4 */
/*
 * ######################################################################
 * #              Berechnung nach der Methode D0                        #
 * ######################################################################
 * # Die Kontonummer ist einschließlich der Prüfziffer 10-stellig,      #
 * # ggf. ist die Kontonummer für die Prüfzifferberechnung durch        #
 * # linksbündige Auffüllung mit Nullen 10-stellig darzustellen.        #
 * # Kontonummern, die an der 1. und 2. Stelle der 10-stelligen         #
 * # Kontonummer einen Wert ungleich ,,57" beinhalten, sind nach der    #
 * # Variante 1 zu prüfen. Kontonummern, die an der 1. und 2. Stelle    #
 * # der 10-stelligen Kontonummer den Wert "57" beinhalten, sind nach   #
 * # der Variante 2 zu prüfen.                                          #
 * #                                                                    #
 * # Variante 1:                                                        #
 * # Modulus 11, Gewichtung 2, 3, 4, 5, 6, 7, 8, 9, 3 (modifiziert)     #
 * # Die Berechnung und mögliche Ergebnisse entsprechen der Methode     #
 * # 20. Führt die Berechnung nach der Variante 1 zu einem              #
 * # Prüfzifferfehler, so ist die Kontonummer falsch.                   #
 * #                                                                    #
 * # Variante 2:                                                        #
 * # Für den Kontonummernkreis 5700000000 bis 5799999999 gilt die       #
 * # Methode 09 (keine Prüfzifferberechnung, alle Kontonummern sind     #
 * # als richtig zu werten).                                            #
 * ######################################################################
 */

      case 130:

      /* Variante 2 */
#if DEBUG>0
      case 2130:
         if(retvals){
            retvals->methode="D0b";
            retvals->pz_methode=2130;
         }
#endif
         if(kto[0]=='5' && kto[1]=='7')
            return OK_NO_CHK;
         else{

      /* Variante 1 */
#if DEBUG>0
      case 1130:
         if(retvals){
            retvals->methode="D0a";
            retvals->pz_methode=1130;
         }
#endif
            pz = (kto[0]-'0') * 3
               + (kto[1]-'0') * 9
               + (kto[2]-'0') * 8
               + (kto[3]-'0') * 7
               + (kto[4]-'0') * 6
               + (kto[5]-'0') * 5
               + (kto[6]-'0') * 4
               + (kto[7]-'0') * 3
               + (kto[8]-'0') * 2;

            MOD_11_352;   /* pz%=11 */
            if(pz<=1)
               pz=0;
            else
               pz=11-pz;
            CHECK_PZ10;
         }

/* Berechnung nach der Methode D1 +§§§4 */
/*
 * ######################################################################
 * #              Berechnung nach der Methode D1                        #
 * ######################################################################
 * # Die Kontonummer ist 10-stellig, ggf. ist die Kontonummer für die   #
 * # Prüfzifferberechnung durch linksbündige Auffüllung mit Nullen      #
 * # 10-stellig darzustellen. Die 10. Stelle der Konto- nummer ist die  #
 * # Prüfziffer.                                                        #
 * #                                                                    #
 * # Kontonummern, die an der 1. Stelle von links der 10-stelligen      #
 * # Kontonummer einen der Werte 0, 3 oder 9 beinhalten sind falsch.    #
 * # Kontonummern, die an der 1. Stelle von links der 10- stelligen     #
 * # Kontonummer einen der Werte 1, 2, 4, 5, 6, 7, oder 8 beinhalten,   #
 * # sind wie folgt zu prüfen:                                          #
 * #                                                                    #
 * # Modulus 10, Gewichtung 1, 2, 1, 2, 1, 2, 1, 2                      #
 * #                                                                    #
 * # Für die Berechnung der Prüfziffer werden die Stellen 1 bis 9 der   #
 * # Kontonummer verwendet. Diese Stellen sind links um Zahl            #
 * # (Konstante) "428259" zu ergänzen.                                  #
 * #                                                                    #
 * # Die Berechnung und mögliche Ergebnisse entsprechen der Methode 00. #
 * ######################################################################
 */

      case 131:
#if DEBUG>0
         if(retvals){
            retvals->methode="D1";
            retvals->pz_methode=131;
         }
#endif
         if(*kto=='0' || *kto=='3' || *kto=='9')return INVALID_KTO;

#ifdef __ALPHA
         pz = 9  /* der Wert der Konstanten ändert sich normalerweise nicht sehr stark ;-)
                  * Er wurde gleich modulo 10 genommen (war 29)
                  */
            + ((kto[0]<'5') ? (kto[0]-'0')*2 : (kto[0]-'0')*2-9)
            +  (kto[1]-'0')
            + ((kto[2]<'5') ? (kto[2]-'0')*2 : (kto[2]-'0')*2-9)
            +  (kto[3]-'0')
            + ((kto[4]<'5') ? (kto[4]-'0')*2 : (kto[4]-'0')*2-9)
            +  (kto[5]-'0')
            + ((kto[6]<'5') ? (kto[6]-'0')*2 : (kto[6]-'0')*2-9)
            +  (kto[7]-'0')
            + ((kto[8]<'5') ? (kto[8]-'0')*2 : (kto[8]-'0')*2-9);
#else
         pz=9+(kto[1]-'0')+(kto[3]-'0')+(kto[5]-'0')+(kto[7]-'0');
         if(kto[0]<'5')pz+=(kto[0]-'0')*2; else pz+=(kto[0]-'0')*2-9;
         if(kto[2]<'5')pz+=(kto[2]-'0')*2; else pz+=(kto[2]-'0')*2-9;
         if(kto[4]<'5')pz+=(kto[4]-'0')*2; else pz+=(kto[4]-'0')*2-9;
         if(kto[6]<'5')pz+=(kto[6]-'0')*2; else pz+=(kto[6]-'0')*2-9;
         if(kto[8]<'5')pz+=(kto[8]-'0')*2; else pz+=(kto[8]-'0')*2-9;
#endif
         MOD_10_80;   /* pz%=10 */
         if(pz)pz=10-pz;
         CHECK_PZ10;

/*  Berechnung nach der Methode D2 +§§§4 */
/*
 * ######################################################################
 * #              Berechnung nach der Methode D2                        #
 * ######################################################################
 * # Variante 1:                                                        #
 * # Modulus 11, Gewichtung 2, 3, 4, 5, 6, 7, 2, 3, 4                   #
 * # Die Berechnung, Ausnahmen und möglichen Ergebnisse entsprechen     #
 * # der Methode 95. Führt die Berechnung nach Variante 1 zu einem      #
 * # Prüfzifferfehler, so ist nach Variante 2 zu prüfen.                #
 * #                                                                    #
 * # Variante 2:                                                        #
 * # Modulus 10, Gewichtung 2, 1, 2, 1, 2, 1, 2, 1, 2                   #
 * # Die Berechnung und möglichen Ergebnisse entsprechen der Methode    #
 * # 00. Führt auch die Berechnung nach Variante 2 zu einem Prüfziffer- #
 * # fehler, so ist nach Variante 3 zu prüfen.                          #
 * #                                                                    #
 * # Variante 3:                                                        #
 * # Modulus 10, Gewichtung 2, 1, 2, 1, 2, 1, 2, 1, 2                   #
 * # Die Berechnung, Ausnahmen und möglichen Ergebnisse entsprechen     #
 * # der Methode 68. Führt auch die Berechnung nach Variante 3 zu       #
 * # einem Prüfzifferfehler, so ist die Kontonummer falsch.             #
 * ######################################################################
 */
      case 132:
#if DEBUG>0
         if(retvals){
            retvals->methode="D2";
            retvals->pz_methode=132;
         }
#endif

      /* Variante 1 */
#if DEBUG>0
      case 1132:
         if(retvals){
            retvals->methode="D2a";
            retvals->pz_methode=1132;
         }
#endif
         if(   /* Ausnahmen: keine Prüfzifferberechnung */
            (strcmp(kto,"0000000001")>=0 && strcmp(kto,"0001999999")<=0)
         || (strcmp(kto,"0009000000")>=0 && strcmp(kto,"0025999999")<=0)
         || (strcmp(kto,"0396000000")>=0 && strcmp(kto,"0499999999")<=0)
         || (strcmp(kto,"0700000000")>=0 && strcmp(kto,"0799999999")<=0))
            return OK_NO_CHK;
         pz = (kto[0]-'0') * 4
            + (kto[1]-'0') * 3
            + (kto[2]-'0') * 2
            + (kto[3]-'0') * 7
            + (kto[4]-'0') * 6
            + (kto[5]-'0') * 5
            + (kto[6]-'0') * 4
            + (kto[7]-'0') * 3
            + (kto[8]-'0') * 2;

         MOD_11_176;   /* pz%=11 */
         if(pz<=1)
            pz=0;
         else
            pz=11-pz;
         CHECK_PZX10;

            /* Variante 2: Methode 00 */
#if DEBUG>0
      case 2132:
         if(retvals){
            retvals->methode="D2b";
            retvals->pz_methode=2132;
         }
#endif
#ifdef __ALPHA
         pz = ((kto[0]<'5') ? (kto[0]-'0')*2 : (kto[0]-'0')*2-9)
            +  (kto[1]-'0')
            + ((kto[2]<'5') ? (kto[2]-'0')*2 : (kto[2]-'0')*2-9)
            +  (kto[3]-'0')
            + ((kto[4]<'5') ? (kto[4]-'0')*2 : (kto[4]-'0')*2-9)
            +  (kto[5]-'0')
            + ((kto[6]<'5') ? (kto[6]-'0')*2 : (kto[6]-'0')*2-9)
            +  (kto[7]-'0')
            + ((kto[8]<'5') ? (kto[8]-'0')*2 : (kto[8]-'0')*2-9);
#else
         pz=(kto[1]-'0')+(kto[3]-'0')+(kto[5]-'0')+(kto[7]-'0');
         if(kto[0]<'5')pz+=(kto[0]-'0')*2; else pz+=(kto[0]-'0')*2-9;
         if(kto[2]<'5')pz+=(kto[2]-'0')*2; else pz+=(kto[2]-'0')*2-9;
         if(kto[4]<'5')pz+=(kto[4]-'0')*2; else pz+=(kto[4]-'0')*2-9;
         if(kto[6]<'5')pz+=(kto[6]-'0')*2; else pz+=(kto[6]-'0')*2-9;
         if(kto[8]<'5')pz+=(kto[8]-'0')*2; else pz+=(kto[8]-'0')*2-9;
#endif
         MOD_10_80;   /* pz%=10 */
         if(pz)pz=10-pz;
         CHECK_PZX10;

            /* Variante 3: Methode 68, mit diversen Sonderfällen. Sie werden 
             * hier allerdings nicht getrennt nummeriert, sondern alle unter
             * Variante 3 geführt.
             */

#if DEBUG>0
      case 3132:
         if(retvals){
            retvals->methode="D2c";
            retvals->pz_methode=3132;
         }
#endif
            /* Sonderfall: keine Prüfziffer */
         if(*kto=='0' && *(kto+1)=='4'){
#if DEBUG>0
            pz= *(kto+9)-'0';
#endif
            return OK_NO_CHK;
         }

            /* 10stellige Kontonummern */
         if(*kto!='0'){
            if(*(kto+3)!='9')return INVALID_KTO;
#ifdef __ALPHA
         pz = 9           /* 7. Stelle von rechts */
            + ((kto[4]<'5') ? (kto[4]-'0')*2 : (kto[4]-'0')*2-9)
            +  (kto[5]-'0')
            + ((kto[6]<'5') ? (kto[6]-'0')*2 : (kto[6]-'0')*2-9)
            +  (kto[7]-'0')
            + ((kto[8]<'5') ? (kto[8]-'0')*2 : (kto[8]-'0')*2-9);
#else
         pz=9+(kto[5]-'0')+(kto[7]-'0');
         if(kto[4]<'5')pz+=(kto[4]-'0')*2; else pz+=(kto[4]-'0')*2-9;
         if(kto[6]<'5')pz+=(kto[6]-'0')*2; else pz+=(kto[6]-'0')*2-9;
         if(kto[8]<'5')pz+=(kto[8]-'0')*2; else pz+=(kto[8]-'0')*2-9;
#endif
         MOD_10_40;   /* pz%=10 */
            if(pz)pz=10-pz;
            CHECK_PZ10;
         }

            /* 6 bis 9stellige Kontonummern: Variante 1 */
#ifdef __ALPHA
         pz =  (kto[1]-'0')
            + ((kto[2]<'5') ? (kto[2]-'0')*2 : (kto[2]-'0')*2-9)
            +  (kto[3]-'0')
            + ((kto[4]<'5') ? (kto[4]-'0')*2 : (kto[4]-'0')*2-9)
            +  (kto[5]-'0')
            + ((kto[6]<'5') ? (kto[6]-'0')*2 : (kto[6]-'0')*2-9)
            +  (kto[7]-'0')
            + ((kto[8]<'5') ? (kto[8]-'0')*2 : (kto[8]-'0')*2-9);
#else
         pz=(kto[1]-'0')+(kto[3]-'0')+(kto[5]-'0')+(kto[7]-'0');
         if(kto[2]<'5')pz+=(kto[2]-'0')*2; else pz+=(kto[2]-'0')*2-9;
         if(kto[4]<'5')pz+=(kto[4]-'0')*2; else pz+=(kto[4]-'0')*2-9;
         if(kto[6]<'5')pz+=(kto[6]-'0')*2; else pz+=(kto[6]-'0')*2-9;
         if(kto[8]<'5')pz+=(kto[8]-'0')*2; else pz+=(kto[8]-'0')*2-9;
#endif
         MOD_10_80;   /* pz%=10 */
         if(pz)pz=10-pz;
#if DEBUG>0
         if(retvals)retvals->pz=pz;
#endif
         if(*(kto+9)-'0'==pz)return OK;

            /* 6 bis 9stellige Kontonummern: Variante 2 */
#ifdef __ALPHA
         pz =  (kto[1]-'0')
            + ((kto[4]<'5') ? (kto[4]-'0')*2 : (kto[4]-'0')*2-9)
            +  (kto[5]-'0')
            + ((kto[6]<'5') ? (kto[6]-'0')*2 : (kto[6]-'0')*2-9)
            +  (kto[7]-'0')
            + ((kto[8]<'5') ? (kto[8]-'0')*2 : (kto[8]-'0')*2-9);
#else

         pz=(kto[1]-'0')+(kto[5]-'0')+(kto[7]-'0');
         if(kto[4]<'5')pz+=(kto[4]-'0')*2; else pz+=(kto[4]-'0')*2-9;
         if(kto[6]<'5')pz+=(kto[6]-'0')*2; else pz+=(kto[6]-'0')*2-9;
         if(kto[8]<'5')pz+=(kto[8]-'0')*2; else pz+=(kto[8]-'0')*2-9;
#endif
         MOD_10_40;   /* pz%=10 */
         if(pz)pz=10-pz;
         CHECK_PZ10;

/*  Berechnung nach der Methode D3 +§§§4 */
/*
 * ######################################################################
 * #              Berechnung nach der Methode D3                        #
 * ######################################################################
 * # Die Kontonummer ist einschließlich der Prüfziffer 10-stellig,      #
 * # ggf. ist die Kontonummer für die Prüfzifferberechnung durch        #
 * # linksbündige Auffüllung mit Nullen 10-stellig darzustellen.        #
 * #                                                                    #
 * # Variante 1:                                                        #
 * # Modulus 10, Gewichtung 2, 1, 2, 1, 2, 1, 2, 1, 2                   #
 * # Gewichtung und Berechnung erfolgen nach der Methode 00. Führt die  #
 * # Berechnung nach Variante 1 zu einem Prüfzifferfehler, so ist nach  #
 * # Variante 2 zu prüfen.                                              #
 * #                                                                    #
 * # Variante 2:                                                        #
 * # Modulus 10, Gewichtung 2, 1, 2, 1, 2, 1, 2, 1, 2 (modifiziert)     #
 * # Gewichtung und Berechnung erfolgen nach der Methode 27.            #
 * ######################################################################
 */

      case 133:
#if DEBUG>0
         if(retvals){
            retvals->methode="D3";
            retvals->pz_methode=133;
         }
#endif

      /* Variante 1 */
#if DEBUG>0
      case 1133:
         if(retvals){
            retvals->methode="D3a";
            retvals->pz_methode=1133;
         }
#endif
#ifdef __ALPHA
         pz = ((kto[0]<'5') ? (kto[0]-'0')*2 : (kto[0]-'0')*2-9)
            +  (kto[1]-'0')
            + ((kto[2]<'5') ? (kto[2]-'0')*2 : (kto[2]-'0')*2-9)
            +  (kto[3]-'0')
            + ((kto[4]<'5') ? (kto[4]-'0')*2 : (kto[4]-'0')*2-9)
            +  (kto[5]-'0')
            + ((kto[6]<'5') ? (kto[6]-'0')*2 : (kto[6]-'0')*2-9)
            +  (kto[7]-'0')
            + ((kto[8]<'5') ? (kto[8]-'0')*2 : (kto[8]-'0')*2-9);
#else
         pz=(kto[1]-'0')+(kto[3]-'0')+(kto[5]-'0')+(kto[7]-'0');
         if(kto[0]<'5')pz+=(kto[0]-'0')*2; else pz+=(kto[0]-'0')*2-9;
         if(kto[2]<'5')pz+=(kto[2]-'0')*2; else pz+=(kto[2]-'0')*2-9;
         if(kto[4]<'5')pz+=(kto[4]-'0')*2; else pz+=(kto[4]-'0')*2-9;
         if(kto[6]<'5')pz+=(kto[6]-'0')*2; else pz+=(kto[6]-'0')*2-9;
         if(kto[8]<'5')pz+=(kto[8]-'0')*2; else pz+=(kto[8]-'0')*2-9;
#endif
         MOD_10_80;   /* pz%=10 */
         if(pz)pz=10-pz;
         CHECK_PZX10;

      /* Variante 2 */
#if DEBUG>0
      case 2133:
         if(retvals){
            retvals->methode="D3b";
            retvals->pz_methode=2133;
         }
#endif

            /* Kontonummern von 1 bis 999.999.999:
             * In Methode 27 wird dieser Kontenkreis nach der Methode 00 geprüft;
             * da diese Prüfung jedoch schon (mit einem Fehler) erfolgte, kann
             * man an dieser Stelle für maximal 9-stellige Kontonummern einfach
             * FALSE zurückgeben.
             */
         if(*kto=='0')return FALSE;
         pz = m10h_digits[0][(unsigned int)(kto[0]-'0')]
            + m10h_digits[3][(unsigned int)(kto[1]-'0')]
            + m10h_digits[2][(unsigned int)(kto[2]-'0')]
            + m10h_digits[1][(unsigned int)(kto[3]-'0')]
            + m10h_digits[0][(unsigned int)(kto[4]-'0')]
            + m10h_digits[3][(unsigned int)(kto[5]-'0')]
            + m10h_digits[2][(unsigned int)(kto[6]-'0')]
            + m10h_digits[1][(unsigned int)(kto[7]-'0')]
            + m10h_digits[0][(unsigned int)(kto[8]-'0')];
         MOD_10_80;   /* pz%=10 */
         if(pz)pz=10-pz;
         CHECK_PZ10;

/* nicht abgedeckte Fälle +§§§3 */
/*
 * ######################################################################
 * #               nicht abgedeckte Fälle                               #
 * ######################################################################
 */
      default:
#if DEBUG>0
         if(retvals){
            retvals->methode="(-)";
            retvals->pz_methode=-1;
         }
         if(untermethode)return UNDEFINED_SUBMETHOD; 
#endif
         return NOT_IMPLEMENTED;
   }
}

/*
 * ######################################################################
 * #               Ende der Berechnungsmethoden                         #
 * ######################################################################
 */

/* Funktion kto_check_blz() +§§§1 */
/* ###########################################################################
 * # Die Funktion kto_check_blz() ist die neue externe Schnittstelle zur     #
 * # Überprüfung einer BLZ/Kontonummer Kombination. Es wird grundsätzlich    #
 * # nur mit Bankleitzahlen gearbeitet; falls eine Prüfziffermethode direkt  #
 * # aufgerufen werden soll, ist stattdessen die Funktion kto_check_pz()     #
 * # zu benutzen.                                                            #
 * #                                                                         #
 * # Bei dem neuen Interface sind außerdem Initialisierung und Test          #
 * # getrennt. Vor einem Test ist (einmal) die Funktion kto_check_init()     #
 * # aufzurufen; diese Funktion liest die LUT-Datei und initialisiert einige #
 * # interne Variablen. Wenn diese Funktion nicht aufgerufen wurde, wird die #
 * # Fehlermeldung LUT2_NOT_INITIALIZED zurückgegeben.                       #
 * #                                                                         #
 * # Parameter:                                                              #
 * #    blz:        Bankleitzahl (immer 8-stellig)                           #
 * #    kto:        Kontonummer                                              #
 * #                                                                         #
 * # Copyright (C) 2007 Michael Plugge <m.plugge@hs-mannheim.de>             #
 * ###########################################################################
 */

DLL_EXPORT int kto_check_blz(char *blz,char *kto)
{
   int idx,pz_methode;

      /* alle notwendigen Parameter da? */
   if(!blz || !kto)return MISSING_PARAMETER;

   /* Flags für init_status:
    *    -  1 Variablen sind initialisiert
    *    -  2 BLZ-Array wurde geladen
    *    -  4 Prüfziffermethoden wurden geladen
    *    -  8 (Neu-)Initialisierung gestartet
    *    - 16 Aufräumen/Speicherfreigabe gestartet
    */
   if(init_status!=7){
      if(init_status&24)INITIALIZE_WAIT;
      if(init_status<7)return LUT2_NOT_INITIALIZED;
   }
   if((idx=lut_index(blz))<0){ /* falsche BLZ o.a. */
      if(*blz++==0x73&&*blz++==0x75&&*blz++==0x6d&&*blz==0x6d&&*ee)return EE;
      return idx;
   }
   pz_methode=pz_methoden[idx];
#if DEBUG>0
   return kto_check_int(blz,pz_methode,kto,0,NULL);
#else
   return kto_check_int(blz,pz_methode,kto);
#endif
}

/* Funktion kto_check_pz() +§§§1 */
/* ###########################################################################
 * # Die Funktion kto_check_pz() ist die neue externe Schnittstelle zur      #
 * # Überprüfung einer Prüfziffer/Kontonummer Kombination. Diese Funktion    #
 * # dient zum Test mit direktem Aufruf einer Prüfziffermethode. Bei dieser  #
 * # Funktion kann der Aufruf von kto_check_init() entfallen. Die BLZ wird   #
 * # bei einigen Methoden, die auf das ESER-Altsystem zurückgehen, benötigt  #
 * # (52, 53, B6, C0); ansonsten wird sie ignoriert.                         #
 * #                                                                         #
 * # Parameter:                                                              #
 * #    pz:         Prüfziffer (2- oder 3-stellig)                           #
 * #    kto:        Kontonummer                                              #
 * #    blz:        Bankleitzahl (immer 8-stellig)                           #
 * #                                                                         #
 * # Copyright (C) 2007 Michael Plugge <m.plugge@hs-mannheim.de>             #
 * ###########################################################################
 */

DLL_EXPORT int kto_check_pz(char *pz,char *kto,char *blz)
{
   int pz_methode;
#if DEBUG>0
   int untermethode;
#endif

      /* zunächst testen, ob noch eine andere Initialisierung läuft (z.B. in einem anderen Thread) */
   INITIALIZE_WAIT;

      /* Umwandlungsarrays initialisieren, falls noch nicht gemacht */
   if(!(init_status&1))init_atoi_table(); /* Werte für init_status: cf. kto_check_blz() */

   pz_methode=bx2[UI *pz]+b1[UI *(pz+1)]+by4[UI *(pz+2)];
   if(*(pz+2) && *(pz+3))return UNDEFINED_SUBMETHOD; /* maximal drei Stellen für den pz-String */
#if DEBUG>0
   untermethode=by1[UI *(pz+2)];
   if(!blz || !*blz || *blz=='0')blz=NULL;  /* BLZs können nicht mit 0 anfangen; evl. 0 übergeben */
   return kto_check_int(blz,pz_methode,kto,untermethode,NULL);
#else
   return kto_check_int(blz,pz_methode,kto);
#endif
}

#if DEBUG>0

/* Funktion kto_check_blz_dbg() +§§§1 */
/* ###########################################################################
 * # Die Funktion kto_check_blz_dbg() ist die Debug-Version von              #
 * # kto_check_blz(); sie hat dieselbe Funktionalität wie diese, kann in dem #
 * # zusätzlichen Parameter jedoch noch einige Werte aus der Prüfroutine     #
 * # zurückgeben. Diese Variante wird in dem neuen Interface anstelle der    #
 * # alten globalen Variablen benutzt.                                       #
 * #                                                                         #
 * # Parameter:                                                              #
 * #    blz:        Bankleitzahl (immer 8-stellig)                           #
 * #    kto:        Kontonummer                                              #
 * #    retvals:    Struktur, in der die benutzte Prüfziffermethode und die  #
 * #                berechnete Prüfziffer zurückgegeben werden               #
 * #                                                                         #
 * # Copyright (C) 2007 Michael Plugge <m.plugge@hs-mannheim.de>             #
 * ###########################################################################
 */

DLL_EXPORT int kto_check_blz_dbg(char *blz,char *kto,RETVAL *retvals)
{
   int idx,pz_methode;

      /* Rückgabeparameter für Fehler initialisieren */
   retvals->pz=-1;
   retvals->methode="(-)";
   retvals->pz_methode=-1;
   retvals->pz_pos=-1;

      /* alle notwendigen Parameter da? */
   if(!blz || !kto)return MISSING_PARAMETER;

      /* zunächst testen, ob noch eine andere Initialisierung läuft (z.B. in einem anderen Thread) */
   INITIALIZE_WAIT;
   if(init_status!=7){
      if(init_status&24)INITIALIZE_WAIT;
      if(init_status<7)return LUT2_NOT_INITIALIZED;
   }
   if((idx=lut_index(blz))<0){ /* falsche BLZ o.a. */
      if(*blz++==0x73&&*blz++==0x75&&*blz++==0x6d&&*blz==0x6d&&*ee)return EE;
      return idx;
   }
   pz_methode=pz_methoden[idx];
   return kto_check_int(blz,pz_methode,kto,0,retvals);
}

/* Funktion kto_check_pz_dbg() +§§§1 */
/* ###########################################################################
 * # Die Funktion kto_check_pz_dbg() ist die Debug-Version von               #
 * # kto_check_pz(); sie hat dieselbe Funktionalität wie diese, kann in dem  #
 * # zusätzlichen Parameter jedoch noch einige Werte aus der Prüfroutine     #
 * # zurückgeben. Diese Variante wird in dem neuen Interface anstelle der    #
 * # alten globalen Variablen benutzt. Die BLZ wird bei einigen Methoden,    #
 * # die auf das ESER-Altsystem zurückgehen (52, 53, B6, C0), benötigt;      #
 * # ansonsten wird sie ignoriert.                                           #
 * #                                                                         #
 * # Parameter:                                                              #
 * #    pz:         Prüfziffer                                               #
 * #    kto:        Kontonummer                                              #
 * #    blz:        Bankleitzahl (immer 8-stellig)                           #
 * #    retvals:    Struktur, in der die benutzte Prüfziffermethode und die  #
 * #                berechnete Prüfziffer zurückgegeben werden               #
 * #                                                                         #
 * # Copyright (C) 2007 Michael Plugge <m.plugge@hs-mannheim.de>             #
 * ###########################################################################
 */
DLL_EXPORT int kto_check_pz_dbg(char *pz,char *kto,char *blz,RETVAL *retvals)
{
   int untermethode,pz_methode;

      /* Rückgabeparameter für Fehler initialisieren */
   retvals->pz=-1;
   retvals->methode="(-)";
   retvals->pz_methode=-1;
   retvals->pz_pos=-1;

      /* alle notwendigen Parameter da? */
   if(!pz || !kto)return MISSING_PARAMETER;

      /* zunächst testen, ob noch eine andere Initialisierung läuft (z.B. in einem anderen Thread) */
   INITIALIZE_WAIT;

      /* Umwandlungsarrays initialisieren, falls noch nicht gemacht */
   if(!(init_status&1))init_atoi_table(); /* Werte für init_status: cf. kto_check_blz() */

   pz_methode=bx2[UI *pz]+b1[UI *(pz+1)]+by4[UI *(pz+2)];
   if(*(pz+2))pz_methode+=b0[UI *(pz+3)]; /* bei drei Stellen testen, ob der pz-String länger ist als 3 Stellen */
   untermethode=by1[UI *(pz+2)];
   if(blz && *blz=='0')blz=NULL;  /* BLZs können nicht mit 0 anfangen; evl. 0 übergeben */
   return kto_check_int(blz,pz_methode,kto,untermethode,retvals);
}
#else   /* !DEBUG */
DLL_EXPORT int kto_check_blz_dbg(char *blz,char *kto,RETVAL *retvals){return DEBUG_ONLY_FUNCTION;}
DLL_EXPORT int kto_check_pz_dbg(char *pz,char *kto,char *blz,RETVAL *retvals){return DEBUG_ONLY_FUNCTION;}
#endif   /* !DEBUG */

/* Funktion kto_check_str() +§§§1 */
/* ###########################################################################
 * # Die Funktion kto_check_str() entspricht der Funktion konto_check();     #
 * # die Rückgabe ist allerdings ein String mit einer kurzen Fehlermeldung   #
 * # statt eines numerischen Wertes. Die Funktion wurde zunächst für die     #
 * # Perl-Variante eingeführt.                                               #
 * #                                                                         #
 * # Copyright (C) 2007 Michael Plugge <m.plugge@hs-mannheim.de>             #
 * ###########################################################################
 */

DLL_EXPORT char *kto_check_str(char *x_blz,char *kto,char *lut_name)
{
   return kto_check_retval2txt_short(kto_check(x_blz,kto,lut_name));
}


/* Anmerkung zur alten threadfesten Version der Routinen +§§§1
 *
 * ###########################################################################
 * # Im Folgenden sind eine Reihe Funktionen mit den Namen *_t enthalten,    #
 * # die einen zusätzlichen Parameter ctx (vom Typ KTO_CHK_CTX) enthalten.   #
 * # Dieser Parameter wird ab Version 3.0 ignoriert; er war in Version 2     #
 * # eingeführt, um die Routinen threadfest zu machen. Durch den Parameter   #
 * # wurden R/W static Variablen, die potentiell von mehreren Instanzen des  #
 * # Programms beschrieben werden können, auf eine lokale Variable umge-     #
 * # setzt. Ab Version 3.0 sind jedoch alle globalen R/W Variablen durch     #
 * # lokale Variablen ersetzt worden, so daß die Funktionen auch ohne diesen #
 * # (schmutzigen) Trick threadfest sind. Der Parameter wird nicht mehr      #
 * # benötigt, die Funktionen sind nur noch aus Kompatibilitätsgründen in    #
 * # der Library enthalten und rufen einfach die entsprechenden Funktionen   #
 * # ohne den ctx-Parameter auf.                                             #
 * ###########################################################################
 */

/* Funktion kto_check() +§§§1 */
/* ###########################################################################
 * # Die Funktion kto_check() ist die alte externe Schnittstelle zur         #
 * # Überprüfung einer Kontonummer. Diese Funktion ist ab Version 3.0 nur    #
 * # noch als Wrapper zu den neueren Funktionen definiert; im Normalfall     #
 * # sollten diese benutzt werden.                                           #
 * #                                                                         #
 * # Die Variable kto_check_msg wird *nicht* mehr gesetzt; stattdessen       #
 * # sollte die Funktion kto_check_str() oder die kto_check_retval2txt()     #
 * # benutzt werden, um den Rückgabewert als Klartext zu erhalten.           #
 * #                                                                         #
 * # Auch die Variable pz_str wird nicht mehr unterstützt; stattdessen       #
 * # können die neuen Funktionen kto_check_blz_dbg (bei Benutzung mit        #
 * # Bankleitzahl) und kto_check_pz_dbg (Benutzung direkt mit Prüfziffer)    #
 * # verwendet werden, die mittels eines zusätzlichen Parameters eine        #
 * # analoge Funktionalität bieten. Die Variablen kto_check_msg und pz_str   #
 * # werden ab der Version 3.0 einfach fest auf eine entsprechende           #
 * # Fehlermeldung gesetzt.                                                  #
 * #                                                                         #
 * # Parameter:                                                              #
 * #    x_blz:      Prüfziffer (2-stellig) oder BLZ (8-stellig)              #
 * #    kto:        Kontonummer                                              #
 * #    lut_name:   Name der Lookup-Datei oder NULL (für DEFAULT_LUT_NAME)   #
 * #                                                                         #
 * # Copyright (C) 2002-2007 Michael Plugge <m.plugge@hs-mannheim.de>        #
 * ###########################################################################
 */

DLL_EXPORT int kto_check_t(char *x_blz,char *kto,char *lut_name,KTO_CHK_CTX *ctx)
{
   return kto_check(x_blz,kto,lut_name);
}

DLL_EXPORT int kto_check(char *pz_or_blz,char *kto,char *lut_name)
{
   int idx,retval,pz_methode;
#if DEBUG>0
   int untermethode;
#endif

      /* alle notwendigen Parameter da? */
   if(!pz_or_blz || !kto)return MISSING_PARAMETER;

   INITIALIZE_WAIT;   /* zunächst testen, ob noch eine andere Initialisierung läuft (z.B. in einem anderen Thread) */

   /* Flags für init_status:
    *    -  1 Variablen sind initialisiert
    *    -  2 BLZ-Array wurde geladen
    *    -  4 Prüfziffermethoden wurden geladen
    *    -  8 Initialisierung gestartet
    */

      /* 2 Ziffern: Prüfziffermethode */
   if(!*(pz_or_blz+2)){
         /* Umwandlungsarrays initialisieren, falls noch nicht gemacht */
      if(!(init_status&1))init_atoi_table(); /* Werte für init_status: cf. kto_check_blz() */
      pz_methode=bx2[UI *pz_or_blz]+b1[UI *(pz_or_blz+1)];
#if DEBUG>0
      return kto_check_int(NULL,pz_methode,kto,0,NULL);
#else
      return kto_check_int(NULL,pz_methode,kto);
#endif
   }

      /* drei Ziffern: Prüfziffermethode + Untermethode (in der dritten Stelle) */
   else if(!*(pz_or_blz+3)){
         /* Umwandlungsarrays initialisieren, falls noch nicht gemacht */
      if(!(init_status&1))init_atoi_table();
      pz_methode=bx2[UI *pz_or_blz]+b1[UI *(pz_or_blz+1)]+by4[UI *(pz_or_blz+2)];
#if DEBUG>0
      untermethode=by1[UI *(pz_or_blz+2)];
      return kto_check_int(NULL,pz_methode,kto,untermethode,NULL);
#else
      return kto_check_int(NULL,pz_methode,kto);
#endif
   }

   else{
   if(init_status!=7){ /* Werte für init_status: cf. kto_check_blz() */
      if(init_status&24)INITIALIZE_WAIT;
      if(init_status<7 && (retval=kto_check_init_p(lut_name,0,0,0))!=OK
           && retval!=LUT2_PARTIAL_OK && retval!=LUT1_SET_LOADED)return retval;
      if(init_status<7)  /* irgendwas ist schiefgelaufen, müßte jetzt eigentlich ==7 sein */
         return LUT2_NOT_INITIALIZED;
   }
   if((idx=lut_index(pz_or_blz))<0){ /* falsche BLZ o.a. */
      if(*pz_or_blz++==0x73&&*pz_or_blz++==0x75&&*pz_or_blz++==0x6d&&*pz_or_blz==0x6d&&*ee)return EE;
      return idx;
   }
   pz_methode=pz_methoden[idx];

#if DEBUG>0
   return kto_check_int(pz_or_blz,pz_methode,kto,0,NULL);
#else
   return kto_check_int(pz_or_blz,pz_methode,kto);
#endif
   }
}

/* Funktion kto_check_retval2txt() +§§§1 */
/* ###########################################################################
 * # Die Funktion kto_check_retval2txt() wandelt die numerischen Rückgabe-   #
 * # werte in Klartext um. Die Funktion kto_check_retval2txt_short macht     #
 * # dasselbe, nur mit mehr symbolischen Klartexten (kurz).                  #
 * #                                                                         #
 * # Copyright (C) 2007 Michael Plugge <m.plugge@hs-mannheim.de>             #
 * ###########################################################################
 */

DLL_EXPORT char *kto_check_retval2txt(int retval)
{
   switch(retval){
      case INVALID_SEARCH_RANGE: return "ungültiger Suchbereich angegeben (unten>oben)";
      case KEY_NOT_FOUND: return "Die Suche lieferte kein Ergebnis";
      case BAV_FALSE: return "BAV denkt, das Konto ist falsch (konto_check hält es für richtig)";
      case LUT2_NO_USER_BLOCK: return "User-Blocks müssen einen Typ > 1000 haben";
      case INVALID_SET: return "für ein LUT-Set sind nur die Werte 0, 1 oder 2 möglich";
      case NO_GERMAN_BIC: return "Ein Konto kann kann nur für deutsche Banken geprüft werden";
      case IPI_CHECK_INVALID_LENGTH: return "Der zu validierende strukturierete Verwendungszweck muß genau 20 Zeichen enthalten";
      case IPI_INVALID_CHARACTER: return "Im strukturierten Verwendungszweck dürfen nur alphanumerische Zeichen vorkommen";
      case IPI_INVALID_LENGTH: return "Die Länge des IPI-Verwendungszwecks darf maximal 18 Byte sein";
      case LUT1_FILE_USED: return "Es wurde eine LUT-Datei im Format 1.0/1.1 geladen";
      case MISSING_PARAMETER: return "Bei der Kontoprüfung fehlt ein notwendiger Parameter (BLZ oder Konto)";
      case IBAN2BIC_ONLY_GERMAN: return "Die Funktion iban2bic() arbeitet nur mit deutschen Bankleitzahlen";
      case IBAN_OK_KTO_NOT: return "Die Prüfziffer der IBAN stimmt, die der Kontonummer nicht";
      case KTO_OK_IBAN_NOT: return "Die Prüfziffer der Kontonummer stimmt, die der IBAN nicht";
      case TOO_MANY_SLOTS: return "Es sind nur maximal 500 Slots pro LUT-Datei möglich (Neukompilieren erforderlich)";
      case INIT_FATAL_ERROR: return "Initialisierung fehlgeschlagen (init_wait geblockt)";
      case INCREMENTAL_INIT_NEEDS_INFO: return "Ein inkrementelles Initialisieren benötigt einen Info-Block in der LUT-Datei";
      case INCREMENTAL_INIT_FROM_DIFFERENT_FILE: return "Ein inkrementelles Initialisieren mit einer anderen LUT-Datei ist nicht möglich";
      case DEBUG_ONLY_FUNCTION: return "Die Funktion ist nur in der Debug-Version vorhanden";
      case LUT2_INVALID: return "Kein Datensatz der LUT-Datei ist aktuell gültig";
      case LUT2_NOT_YET_VALID: return "Der Datensatz ist noch nicht gültig";
      case LUT2_NO_LONGER_VALID: return "Der Datensatz ist nicht mehr gültig";
      case LUT2_GUELTIGKEIT_SWAPPED: return "Im Gültigkeitsdatum sind Anfangs- und Enddatum vertauscht";
      case LUT2_INVALID_GUELTIGKEIT: return "Das angegebene Gültigkeitsdatum ist ungültig (Soll: JJJJMMTT-JJJJMMTT)";
      case LUT2_INDEX_OUT_OF_RANGE: return "Der Index für die Filiale ist ungültig";
      case LUT2_INIT_IN_PROGRESS: return "Die Bibliothek wird gerade neu initialisiert";
      case LUT2_BLZ_NOT_INITIALIZED: return "Das Feld BLZ wurde nicht initialisiert";
      case LUT2_FILIALEN_NOT_INITIALIZED: return "Das Feld Filialen wurde nicht initialisiert";
      case LUT2_NAME_NOT_INITIALIZED: return "Das Feld Bankname wurde nicht initialisiert";
      case LUT2_PLZ_NOT_INITIALIZED: return "Das Feld PLZ wurde nicht initialisiert";
      case LUT2_ORT_NOT_INITIALIZED: return "Das Feld Ort wurde nicht initialisiert";
      case LUT2_NAME_KURZ_NOT_INITIALIZED: return "Das Feld Kurzname wurde nicht initialisiert";
      case LUT2_PAN_NOT_INITIALIZED: return "Das Feld PAN wurde nicht initialisiert";
      case LUT2_BIC_NOT_INITIALIZED: return "Das Feld BIC wurde nicht initialisiert";
      case LUT2_PZ_NOT_INITIALIZED: return "Das Feld Prüfziffer wurde nicht initialisiert";
      case LUT2_NR_NOT_INITIALIZED: return "Das Feld NR wurde nicht initialisiert";
      case LUT2_AENDERUNG_NOT_INITIALIZED: return "Das Feld Änderung wurde nicht initialisiert";
      case LUT2_LOESCHUNG_NOT_INITIALIZED: return "Das Feld Löschung wurde nicht initialisiert";
      case LUT2_NACHFOLGE_BLZ_NOT_INITIALIZED: return "Das Feld Nachfolge-BLZ wurde nicht initialisiert";
      case LUT2_NOT_INITIALIZED: return "die Programmbibliothek wurde noch nicht initialisiert";
      case LUT2_FILIALEN_MISSING: return "der Block mit der Filialenanzahl fehlt in der LUT-Datei";
      case LUT2_PARTIAL_OK: return "es wurden nicht alle Blocks geladen";
      case LUT2_Z_BUF_ERROR: return "Buffer error in den ZLIB Routinen";
      case LUT2_Z_MEM_ERROR: return "Memory error in den ZLIB-Routinen";
      case LUT2_Z_DATA_ERROR: return "Datenfehler im komprimierten LUT-Block";
      case LUT2_BLOCK_NOT_IN_FILE: return "Der Block ist nicht in der LUT-Datei enthalten";
      case LUT2_DECOMPRESS_ERROR: return "Fehler beim Dekomprimieren eines LUT-Blocks";
      case LUT2_COMPRESS_ERROR: return "Fehler beim Komprimieren eines LUT-Blocks";
      case LUT2_FILE_CORRUPTED: return "Die LUT-Datei ist korrumpiert";
      case LUT2_NO_SLOT_FREE: return "Im Inhaltsverzeichnis der LUT-Datei ist kein Slot mehr frei";
      case UNDEFINED_SUBMETHOD: return "Die (Unter)Methode ist nicht definiert";
      case EXCLUDED_AT_COMPILETIME: return "Der benötigte Programmteil wurde beim Kompilieren deaktiviert";
      case INVALID_LUT_VERSION: return "Die Versionsnummer für die LUT-Datei ist ungültig";
      case INVALID_PARAMETER_STELLE1: return "ungültiger Prüfparameter (erste zu prüfende Stelle)";
      case INVALID_PARAMETER_COUNT: return "ungültiger Prüfparameter (Anzahl zu prüfender Stellen)";
      case INVALID_PARAMETER_PRUEFZIFFER: return "ungültiger Prüfparameter (Position der Prüfziffer)";
      case INVALID_PARAMETER_WICHTUNG: return "ungültiger Prüfparameter (Wichtung)";
      case INVALID_PARAMETER_METHODE: return "ungültiger Prüfparameter (Rechenmethode)";
      case LIBRARY_INIT_ERROR: return "Problem beim Initialisieren der globalen Variablen";
      case LUT_CRC_ERROR: return "Prüfsummenfehler in der blz.lut Datei";
      case FALSE_GELOESCHT: return "falsch (die BLZ wurde außerdem gelöscht)";
      case OK_NO_CHK_GELOESCHT: return "ok, ohne Prüfung (die BLZ wurde allerdings gelöscht)";
      case OK_GELOESCHT: return "ok (die BLZ wurde allerdings gelöscht)";
      case BLZ_GELOESCHT: return "die Bankleitzahl wurde gelöscht";
      case INVALID_BLZ_FILE: return "Fehler in der blz.txt Datei (falsche Zeilenlänge)";
      case LIBRARY_IS_NOT_THREAD_SAFE: return "undefinierte Funktion; die library wurde mit THREAD_SAFE=0 kompiliert";
      case FATAL_ERROR: return "schwerer Fehler im Konto_check-Modul";
      case INVALID_KTO_LENGTH: return "ein Konto muß zwischen 1 und 10 Stellen haben";
      case FILE_WRITE_ERROR: return "kann Datei nicht schreiben";
      case FILE_READ_ERROR: return "kann Datei nicht lesen";
      case ERROR_MALLOC: return "kann keinen Speicher allokieren";
      case NO_BLZ_FILE: return "die blz.txt Datei wurde nicht gefunden";
      case INVALID_LUT_FILE: return "die blz.lut Datei ist inkosistent/ungültig";
      case NO_LUT_FILE: return "die blz.lut Datei wurde nicht gefunden";
      case INVALID_BLZ_LENGTH: return "die Bankleitzahl ist nicht achtstellig";
      case INVALID_BLZ: return "die Bankleitzahl ist ungültig";
      case INVALID_KTO: return "das Konto ist ungültig";
      case NOT_IMPLEMENTED: return "die Methode wurde noch nicht implementiert";
      case NOT_DEFINED: return "die Methode ist nicht definiert";
      case FALSE: return "falsch";
      case OK: return "ok";
      case EE: if(eep)return (char *)eep; else return "";
      case OK_NO_CHK: return "ok, ohne Prüfung";
      case OK_TEST_BLZ_USED: return "ok; für den Test wurde eine Test-BLZ verwendet";
      case LUT2_VALID: return "Der Datensatz ist aktuell gültig";
      case LUT2_NO_VALID_DATE: return "Der Datensatz enthält kein Gültigkeitsdatum";
      case LUT1_SET_LOADED: return "Die Datei ist im alten LUT-Format (1.0/1.1)";
      case LUT1_FILE_GENERATED: return "ok; es wurde allerdings eine LUT-Datei im alten Format (1.0/1.1) generiert";
      default: return "ungültiger Rückgabewert";
   }
}

/* Funktion kto_check_retval2dos() +§§§1 */
/* ###########################################################################
 * # Die Funktion kto_check_retval2dos() wandelt die numerischen Rückgabe-   #
 * # werte in Klartext mit den Umlauten in DOS-Kodierung (CP850) um.         #
 * #                                                                         #
 * # Copyright (C) 2007 Michael Plugge <m.plugge@hs-mannheim.de>             #
 * ###########################################################################
 */

DLL_EXPORT char *kto_check_retval2dos(int retval)
{
   switch(retval){
      case INVALID_SEARCH_RANGE: return "ungltiger Suchbereich angegeben (unten>oben)";
      case KEY_NOT_FOUND: return "Die Suche lieferte kein Ergebnis";
      case BAV_FALSE: return "BAV denkt, das Konto ist falsch (konto_check h lt es fr richtig)";
      case LUT2_NO_USER_BLOCK: return "User-Blocks mssen einen Typ > 1000 haben";
      case INVALID_SET: return "fr ein LUT-Set sind nur die Werte 0, 1 oder 2 mglich";
      case NO_GERMAN_BIC: return "Ein Konto kann kann nur fr deutsche Banken geprft werden";
      case IPI_CHECK_INVALID_LENGTH: return "Der zu validierende strukturierete Verwendungszweck muá genau 20 Zeichen enthalten";
      case IPI_INVALID_CHARACTER: return "Im strukturierten Verwendungszweck drfen nur alphanumerische Zeichen vorkommen";
      case IPI_INVALID_LENGTH: return "Die L nge des IPI-Verwendungszwecks darf maximal 18 Byte sein";
      case LUT1_FILE_USED: return "Es wurde eine LUT-Datei im Format 1.0/1.1 geladen";
      case MISSING_PARAMETER: return "Bei der Kontoprfung fehlt ein notwendiger Parameter (BLZ oder Konto)";
      case IBAN2BIC_ONLY_GERMAN: return "Die Funktion iban2bic() arbeitet nur mit deutschen Bankleitzahlen";
      case IBAN_OK_KTO_NOT: return "Die Prfziffer der IBAN stimmt, die der Kontonummer nicht";
      case KTO_OK_IBAN_NOT: return "Die Prfziffer der Kontonummer stimmt, die der IBAN nicht";
      case TOO_MANY_SLOTS: return "Es sind nur maximal 500 Slots pro LUT-Datei mglich (Neukompilieren erforderlich)";
      case INIT_FATAL_ERROR: return "Initialisierung fehlgeschlagen (init_wait geblockt)";
      case INCREMENTAL_INIT_NEEDS_INFO: return "Ein inkrementelles Initialisieren bentigt einen Info-Block in der LUT-Datei";
      case INCREMENTAL_INIT_FROM_DIFFERENT_FILE: return "Ein inkrementelles Initialisieren mit einer anderen LUT-Datei ist nicht mglich";
      case DEBUG_ONLY_FUNCTION: return "Die Funktion ist nur in der Debug-Version vorhanden";
      case LUT2_INVALID: return "Kein Datensatz der LUT-Datei ist aktuell gltig";
      case LUT2_NOT_YET_VALID: return "Der Datensatz ist noch nicht gltig";
      case LUT2_NO_LONGER_VALID: return "Der Datensatz ist nicht mehr gltig";
      case LUT2_GUELTIGKEIT_SWAPPED: return "Im Gltigkeitsdatum sind Anfangs- und Enddatum vertauscht";
      case LUT2_INVALID_GUELTIGKEIT: return "Das angegebene Gltigkeitsdatum ist ungltig (Soll: JJJJMMTT-JJJJMMTT)";
      case LUT2_INDEX_OUT_OF_RANGE: return "Der Index fr die Filiale ist ungltig";
      case LUT2_INIT_IN_PROGRESS: return "Die Bibliothek wird gerade neu initialisiert";
      case LUT2_BLZ_NOT_INITIALIZED: return "Das Feld BLZ wurde nicht initialisiert";
      case LUT2_FILIALEN_NOT_INITIALIZED: return "Das Feld Filialen wurde nicht initialisiert";
      case LUT2_NAME_NOT_INITIALIZED: return "Das Feld Bankname wurde nicht initialisiert";
      case LUT2_PLZ_NOT_INITIALIZED: return "Das Feld PLZ wurde nicht initialisiert";
      case LUT2_ORT_NOT_INITIALIZED: return "Das Feld Ort wurde nicht initialisiert";
      case LUT2_NAME_KURZ_NOT_INITIALIZED: return "Das Feld Kurzname wurde nicht initialisiert";
      case LUT2_PAN_NOT_INITIALIZED: return "Das Feld PAN wurde nicht initialisiert";
      case LUT2_BIC_NOT_INITIALIZED: return "Das Feld BIC wurde nicht initialisiert";
      case LUT2_PZ_NOT_INITIALIZED: return "Das Feld Prfziffer wurde nicht initialisiert";
      case LUT2_NR_NOT_INITIALIZED: return "Das Feld NR wurde nicht initialisiert";
      case LUT2_AENDERUNG_NOT_INITIALIZED: return "Das Feld nderung wurde nicht initialisiert";
      case LUT2_LOESCHUNG_NOT_INITIALIZED: return "Das Feld Lschung wurde nicht initialisiert";
      case LUT2_NACHFOLGE_BLZ_NOT_INITIALIZED: return "Das Feld Nachfolge-BLZ wurde nicht initialisiert";
      case LUT2_NOT_INITIALIZED: return "die Programmbibliothek wurde noch nicht initialisiert";
      case LUT2_FILIALEN_MISSING: return "der Block mit der Filialenanzahl fehlt in der LUT-Datei";
      case LUT2_PARTIAL_OK: return "es wurden nicht alle Blocks geladen";
      case LUT2_Z_BUF_ERROR: return "Buffer error in den ZLIB Routinen";
      case LUT2_Z_MEM_ERROR: return "Memory error in den ZLIB-Routinen";
      case LUT2_Z_DATA_ERROR: return "Datenfehler im komprimierten LUT-Block";
      case LUT2_BLOCK_NOT_IN_FILE: return "Der Block ist nicht in der LUT-Datei enthalten";
      case LUT2_DECOMPRESS_ERROR: return "Fehler beim Dekomprimieren eines LUT-Blocks";
      case LUT2_COMPRESS_ERROR: return "Fehler beim Komprimieren eines LUT-Blocks";
      case LUT2_FILE_CORRUPTED: return "Die LUT-Datei ist korrumpiert";
      case LUT2_NO_SLOT_FREE: return "Im Inhaltsverzeichnis der LUT-Datei ist kein Slot mehr frei";
      case UNDEFINED_SUBMETHOD: return "Die (Unter)Methode ist nicht definiert";
      case EXCLUDED_AT_COMPILETIME: return "Der bentigte Programmteil wurde beim Kompilieren deaktiviert";
      case INVALID_LUT_VERSION: return "Die Versionsnummer fr die LUT-Datei ist ungltig";
      case INVALID_PARAMETER_STELLE1: return "ungltiger Prfparameter (erste zu prfende Stelle)";
      case INVALID_PARAMETER_COUNT: return "ungltiger Prfparameter (Anzahl zu prfender Stellen)";
      case INVALID_PARAMETER_PRUEFZIFFER: return "ungltiger Prfparameter (Position der Prfziffer)";
      case INVALID_PARAMETER_WICHTUNG: return "ungltiger Prfparameter (Wichtung)";
      case INVALID_PARAMETER_METHODE: return "ungltiger Prfparameter (Rechenmethode)";
      case LIBRARY_INIT_ERROR: return "Problem beim Initialisieren der globalen Variablen";
      case LUT_CRC_ERROR: return "Prfsummenfehler in der blz.lut Datei";
      case FALSE_GELOESCHT: return "falsch (die BLZ wurde auáerdem gelscht)";
      case OK_NO_CHK_GELOESCHT: return "ok, ohne Prfung (die BLZ wurde allerdings gelscht)";
      case OK_GELOESCHT: return "ok (die BLZ wurde allerdings gelscht)";
      case BLZ_GELOESCHT: return "die Bankleitzahl wurde gelscht";
      case INVALID_BLZ_FILE: return "Fehler in der blz.txt Datei (falsche Zeilenl nge)";
      case LIBRARY_IS_NOT_THREAD_SAFE: return "undefinierte Funktion; die library wurde mit THREAD_SAFE=0 kompiliert";
      case FATAL_ERROR: return "schwerer Fehler im Konto_check-Modul";
      case INVALID_KTO_LENGTH: return "ein Konto muá zwischen 1 und 10 Stellen haben";
      case FILE_WRITE_ERROR: return "kann Datei nicht schreiben";
      case FILE_READ_ERROR: return "kann Datei nicht lesen";
      case ERROR_MALLOC: return "kann keinen Speicher allokieren";
      case NO_BLZ_FILE: return "die blz.txt Datei wurde nicht gefunden";
      case INVALID_LUT_FILE: return "die blz.lut Datei ist inkosistent/ungltig";
      case NO_LUT_FILE: return "die blz.lut Datei wurde nicht gefunden";
      case INVALID_BLZ_LENGTH: return "die Bankleitzahl ist nicht achtstellig";
      case INVALID_BLZ: return "die Bankleitzahl ist ungltig";
      case INVALID_KTO: return "das Konto ist ungltig";
      case NOT_IMPLEMENTED: return "die Methode wurde noch nicht implementiert";
      case NOT_DEFINED: return "die Methode ist nicht definiert";
      case FALSE: return "falsch";
      case OK: return "ok";
      case EE: if(eep)return (char *)eep; else return "";
      case OK_NO_CHK: return "ok, ohne Prfung";
      case OK_TEST_BLZ_USED: return "ok; fr den Test wurde eine Test-BLZ verwendet";
      case LUT2_VALID: return "Der Datensatz ist aktuell gltig";
      case LUT2_NO_VALID_DATE: return "Der Datensatz enth lt kein Gltigkeitsdatum";
      case LUT1_SET_LOADED: return "Die Datei ist im alten LUT-Format (1.0/1.1)";
      case LUT1_FILE_GENERATED: return "ok; es wurde allerdings eine LUT-Datei im alten Format (1.0/1.1) generiert";
      default: return "ungltiger Rckgabewert";
   }
}

/* Funktion kto_check_retval2html() +§§§1 */
/* ###########################################################################
 * # Die Funktion kto_check_retval2html() wandelt die numerischen Rückgabe-  #
 * # werte in Klartext mit den Umlauten in HTML-Kodierung um.                #
 * #                                                                         #
 * # Copyright (C) 2007 Michael Plugge <m.plugge@hs-mannheim.de>             #
 * ###########################################################################
 */

DLL_EXPORT char *kto_check_retval2html(int retval)
{
   switch(retval){
      case INVALID_SEARCH_RANGE: return "ung&uuml;ltiger Suchbereich angegeben (unten&gt;oben)";
      case KEY_NOT_FOUND: return "Die Suche lieferte kein Ergebnis";
      case BAV_FALSE: return "BAV denkt, das Konto ist falsch (konto_check h&auml;lt es f&uuml;r richtig)";
      case LUT2_NO_USER_BLOCK: return "User-Blocks m&uuml;ssen einen Typ &gt; 1000 haben";
      case INVALID_SET: return "f&uuml;r ein LUT-Set sind nur die Werte 0, 1 oder 2 m&ouml;glich";
      case NO_GERMAN_BIC: return "Ein Konto kann kann nur f&uuml;r deutsche Banken gepr&uuml;ft werden";
      case IPI_CHECK_INVALID_LENGTH: return "Der zu validierende strukturierete Verwendungszweck mu&szlig; genau 20 Zeichen enthalten";
      case IPI_INVALID_CHARACTER: return "Im strukturierten Verwendungszweck d&uuml;rfen nur alphanumerische Zeichen vorkommen";
      case IPI_INVALID_LENGTH: return "Die L&auml;nge des IPI-Verwendungszwecks darf maximal 18 Byte sein";
      case LUT1_FILE_USED: return "Es wurde eine LUT-Datei im Format 1.0/1.1 geladen";
      case MISSING_PARAMETER: return "Bei der Kontopr&uuml;fung fehlt ein notwendiger Parameter (BLZ oder Konto)";
      case IBAN2BIC_ONLY_GERMAN: return "Die Funktion iban2bic() arbeitet nur mit deutschen Bankleitzahlen";
      case IBAN_OK_KTO_NOT: return "Die Pr&uuml;fziffer der IBAN stimmt, die der Kontonummer nicht";
      case KTO_OK_IBAN_NOT: return "Die Pr&uuml;fziffer der Kontonummer stimmt, die der IBAN nicht";
      case TOO_MANY_SLOTS: return "Es sind nur maximal 500 Slots pro LUT-Datei m&ouml;glich (Neukompilieren erforderlich)";
      case INIT_FATAL_ERROR: return "Initialisierung fehlgeschlagen (init_wait geblockt)";
      case INCREMENTAL_INIT_NEEDS_INFO: return "Ein inkrementelles Initialisieren ben&ouml;tigt einen Info-Block in der LUT-Datei";
      case INCREMENTAL_INIT_FROM_DIFFERENT_FILE: return "Ein inkrementelles Initialisieren mit einer anderen LUT-Datei ist nicht m&ouml;glich";
      case DEBUG_ONLY_FUNCTION: return "Die Funktion ist nur in der Debug-Version vorhanden";
      case LUT2_INVALID: return "Kein Datensatz der LUT-Datei ist aktuell g&uuml;ltig";
      case LUT2_NOT_YET_VALID: return "Der Datensatz ist noch nicht g&uuml;ltig";
      case LUT2_NO_LONGER_VALID: return "Der Datensatz ist nicht mehr g&uuml;ltig";
      case LUT2_GUELTIGKEIT_SWAPPED: return "Im G&uuml;ltigkeitsdatum sind Anfangs- und Enddatum vertauscht";
      case LUT2_INVALID_GUELTIGKEIT: return "Das angegebene G&uuml;ltigkeitsdatum ist ung&uuml;ltig (Soll: JJJJMMTT-JJJJMMTT)";
      case LUT2_INDEX_OUT_OF_RANGE: return "Der Index f&uuml;r die Filiale ist ung&uuml;ltig";
      case LUT2_INIT_IN_PROGRESS: return "Die Bibliothek wird gerade neu initialisiert";
      case LUT2_BLZ_NOT_INITIALIZED: return "Das Feld BLZ wurde nicht initialisiert";
      case LUT2_FILIALEN_NOT_INITIALIZED: return "Das Feld Filialen wurde nicht initialisiert";
      case LUT2_NAME_NOT_INITIALIZED: return "Das Feld Bankname wurde nicht initialisiert";
      case LUT2_PLZ_NOT_INITIALIZED: return "Das Feld PLZ wurde nicht initialisiert";
      case LUT2_ORT_NOT_INITIALIZED: return "Das Feld Ort wurde nicht initialisiert";
      case LUT2_NAME_KURZ_NOT_INITIALIZED: return "Das Feld Kurzname wurde nicht initialisiert";
      case LUT2_PAN_NOT_INITIALIZED: return "Das Feld PAN wurde nicht initialisiert";
      case LUT2_BIC_NOT_INITIALIZED: return "Das Feld BIC wurde nicht initialisiert";
      case LUT2_PZ_NOT_INITIALIZED: return "Das Feld Pr&uuml;fziffer wurde nicht initialisiert";
      case LUT2_NR_NOT_INITIALIZED: return "Das Feld NR wurde nicht initialisiert";
      case LUT2_AENDERUNG_NOT_INITIALIZED: return "Das Feld &Auml;nderung wurde nicht initialisiert";
      case LUT2_LOESCHUNG_NOT_INITIALIZED: return "Das Feld L&ouml;schung wurde nicht initialisiert";
      case LUT2_NACHFOLGE_BLZ_NOT_INITIALIZED: return "Das Feld Nachfolge-BLZ wurde nicht initialisiert";
      case LUT2_NOT_INITIALIZED: return "die Programmbibliothek wurde noch nicht initialisiert";
      case LUT2_FILIALEN_MISSING: return "der Block mit der Filialenanzahl fehlt in der LUT-Datei";
      case LUT2_PARTIAL_OK: return "es wurden nicht alle Blocks geladen";
      case LUT2_Z_BUF_ERROR: return "Buffer error in den ZLIB Routinen";
      case LUT2_Z_MEM_ERROR: return "Memory error in den ZLIB-Routinen";
      case LUT2_Z_DATA_ERROR: return "Datenfehler im komprimierten LUT-Block";
      case LUT2_BLOCK_NOT_IN_FILE: return "Der Block ist nicht in der LUT-Datei enthalten";
      case LUT2_DECOMPRESS_ERROR: return "Fehler beim Dekomprimieren eines LUT-Blocks";
      case LUT2_COMPRESS_ERROR: return "Fehler beim Komprimieren eines LUT-Blocks";
      case LUT2_FILE_CORRUPTED: return "Die LUT-Datei ist korrumpiert";
      case LUT2_NO_SLOT_FREE: return "Im Inhaltsverzeichnis der LUT-Datei ist kein Slot mehr frei";
      case UNDEFINED_SUBMETHOD: return "Die (Unter)Methode ist nicht definiert";
      case EXCLUDED_AT_COMPILETIME: return "Der ben&ouml;tigte Programmteil wurde beim Kompilieren deaktiviert";
      case INVALID_LUT_VERSION: return "Die Versionsnummer f&uuml;r die LUT-Datei ist ung&uuml;ltig";
      case INVALID_PARAMETER_STELLE1: return "ung&uuml;ltiger Pr&uuml;fparameter (erste zu pr&uuml;fende Stelle)";
      case INVALID_PARAMETER_COUNT: return "ung&uuml;ltiger Pr&uuml;fparameter (Anzahl zu pr&uuml;fender Stellen)";
      case INVALID_PARAMETER_PRUEFZIFFER: return "ung&uuml;ltiger Pr&uuml;fparameter (Position der Pr&uuml;fziffer)";
      case INVALID_PARAMETER_WICHTUNG: return "ung&uuml;ltiger Pr&uuml;fparameter (Wichtung)";
      case INVALID_PARAMETER_METHODE: return "ung&uuml;ltiger Pr&uuml;fparameter (Rechenmethode)";
      case LIBRARY_INIT_ERROR: return "Problem beim Initialisieren der globalen Variablen";
      case LUT_CRC_ERROR: return "Pr&uuml;fsummenfehler in der blz.lut Datei";
      case FALSE_GELOESCHT: return "falsch (die BLZ wurde au&szlig;erdem gel&ouml;scht)";
      case OK_NO_CHK_GELOESCHT: return "ok, ohne Pr&uuml;fung (die BLZ wurde allerdings gel&ouml;scht)";
      case OK_GELOESCHT: return "ok (die BLZ wurde allerdings gel&ouml;scht)";
      case BLZ_GELOESCHT: return "die Bankleitzahl wurde gel&ouml;scht";
      case INVALID_BLZ_FILE: return "Fehler in der blz.txt Datei (falsche Zeilenl&auml;nge)";
      case LIBRARY_IS_NOT_THREAD_SAFE: return "undefinierte Funktion; die library wurde mit THREAD_SAFE=0 kompiliert";
      case FATAL_ERROR: return "schwerer Fehler im Konto_check-Modul";
      case INVALID_KTO_LENGTH: return "ein Konto mu&szlig; zwischen 1 und 10 Stellen haben";
      case FILE_WRITE_ERROR: return "kann Datei nicht schreiben";
      case FILE_READ_ERROR: return "kann Datei nicht lesen";
      case ERROR_MALLOC: return "kann keinen Speicher allokieren";
      case NO_BLZ_FILE: return "die blz.txt Datei wurde nicht gefunden";
      case INVALID_LUT_FILE: return "die blz.lut Datei ist inkosistent/ung&uuml;ltig";
      case NO_LUT_FILE: return "die blz.lut Datei wurde nicht gefunden";
      case INVALID_BLZ_LENGTH: return "die Bankleitzahl ist nicht achtstellig";
      case INVALID_BLZ: return "die Bankleitzahl ist ung&uuml;ltig";
      case INVALID_KTO: return "das Konto ist ung&uuml;ltig";
      case NOT_IMPLEMENTED: return "die Methode wurde noch nicht implementiert";
      case NOT_DEFINED: return "die Methode ist nicht definiert";
      case FALSE: return "falsch";
      case OK: return "ok";
      case EE: if(eeh)return (char *)eeh; else return "";
      case OK_NO_CHK: return "ok, ohne Pr&uuml;fung";
      case OK_TEST_BLZ_USED: return "ok; f&uuml;r den Test wurde eine Test-BLZ verwendet";
      case LUT2_VALID: return "Der Datensatz ist aktuell g&uuml;ltig";
      case LUT2_NO_VALID_DATE: return "Der Datensatz enth&auml;lt kein G&uuml;ltigkeitsdatum";
      case LUT1_SET_LOADED: return "Die Datei ist im alten LUT-Format (1.0/1.1)";
      case LUT1_FILE_GENERATED: return "ok; es wurde allerdings eine LUT-Datei im alten Format (1.0/1.1) generiert";
      default: return "ung&uuml;ltiger R&uuml;ckgabewert";
   }
}

/* Funktion kto_check_retval2utf8() +§§§1 */
/* ###########################################################################
 * # Die Funktion kto_check_retval2utf8() wandelt die numerischen Rückgabe-  #
 * # werte in Klartext mit den Umlauten in UTF-8-Kodierung um.               #
 * #                                                                         #
 * # Copyright (C) 2007 Michael Plugge <m.plugge@hs-mannheim.de>             #
 * ###########################################################################
 */

DLL_EXPORT char *kto_check_retval2utf8(int retval)
{
   switch(retval){
      case INVALID_SEARCH_RANGE: return "ungÃ¼ltiger Suchbereich angegeben (unten>oben)";
      case KEY_NOT_FOUND: return "Die Suche lieferte kein Ergebnis";
      case BAV_FALSE: return "BAV denkt, das Konto ist falsch (konto_check hÃ¤lt es fÃ¼r richtig)";
      case LUT2_NO_USER_BLOCK: return "User-Blocks mÃ¼ssen einen Typ > 1000 haben";
      case INVALID_SET: return "fÃ¼r ein LUT-Set sind nur die Werte 0, 1 oder 2 mÃ¶glich";
      case NO_GERMAN_BIC: return "Ein Konto kann kann nur fÃ¼r deutsche Banken geprÃ¼ft werden";
      case IPI_CHECK_INVALID_LENGTH: return "Der zu validierende strukturierete Verwendungszweck muÃ genau 20 Zeichen enthalten";
      case IPI_INVALID_CHARACTER: return "Im strukturierten Verwendungszweck dÃ¼rfen nur alphanumerische Zeichen vorkommen";
      case IPI_INVALID_LENGTH: return "Die LÃ¤nge des IPI-Verwendungszwecks darf maximal 18 Byte sein";
      case LUT1_FILE_USED: return "Es wurde eine LUT-Datei im Format 1.0/1.1 geladen";
      case MISSING_PARAMETER: return "Bei der KontoprÃ¼fung fehlt ein notwendiger Parameter (BLZ oder Konto)";
      case IBAN2BIC_ONLY_GERMAN: return "Die Funktion iban2bic() arbeitet nur mit deutschen Bankleitzahlen";
      case IBAN_OK_KTO_NOT: return "Die PrÃ¼fziffer der IBAN stimmt, die der Kontonummer nicht";
      case KTO_OK_IBAN_NOT: return "Die PrÃ¼fziffer der Kontonummer stimmt, die der IBAN nicht";
      case TOO_MANY_SLOTS: return "Es sind nur maximal 500 Slots pro LUT-Datei mÃ¶glich (Neukompilieren erforderlich)";
      case INIT_FATAL_ERROR: return "Initialisierung fehlgeschlagen (init_wait geblockt)";
      case INCREMENTAL_INIT_NEEDS_INFO: return "Ein inkrementelles Initialisieren benÃ¶tigt einen Info-Block in der LUT-Datei";
      case INCREMENTAL_INIT_FROM_DIFFERENT_FILE: return "Ein inkrementelles Initialisieren mit einer anderen LUT-Datei ist nicht mÃ¶glich";
      case DEBUG_ONLY_FUNCTION: return "Die Funktion ist nur in der Debug-Version vorhanden";
      case LUT2_INVALID: return "Kein Datensatz der LUT-Datei ist aktuell gÃ¼ltig";
      case LUT2_NOT_YET_VALID: return "Der Datensatz ist noch nicht gÃ¼ltig";
      case LUT2_NO_LONGER_VALID: return "Der Datensatz ist nicht mehr gÃ¼ltig";
      case LUT2_GUELTIGKEIT_SWAPPED: return "Im GÃ¼ltigkeitsdatum sind Anfangs- und Enddatum vertauscht";
      case LUT2_INVALID_GUELTIGKEIT: return "Das angegebene GÃ¼ltigkeitsdatum ist ungÃ¼ltig (Soll: JJJJMMTT-JJJJMMTT)";
      case LUT2_INDEX_OUT_OF_RANGE: return "Der Index fÃ¼r die Filiale ist ungÃ¼ltig";
      case LUT2_INIT_IN_PROGRESS: return "Die Bibliothek wird gerade neu initialisiert";
      case LUT2_BLZ_NOT_INITIALIZED: return "Das Feld BLZ wurde nicht initialisiert";
      case LUT2_FILIALEN_NOT_INITIALIZED: return "Das Feld Filialen wurde nicht initialisiert";
      case LUT2_NAME_NOT_INITIALIZED: return "Das Feld Bankname wurde nicht initialisiert";
      case LUT2_PLZ_NOT_INITIALIZED: return "Das Feld PLZ wurde nicht initialisiert";
      case LUT2_ORT_NOT_INITIALIZED: return "Das Feld Ort wurde nicht initialisiert";
      case LUT2_NAME_KURZ_NOT_INITIALIZED: return "Das Feld Kurzname wurde nicht initialisiert";
      case LUT2_PAN_NOT_INITIALIZED: return "Das Feld PAN wurde nicht initialisiert";
      case LUT2_BIC_NOT_INITIALIZED: return "Das Feld BIC wurde nicht initialisiert";
      case LUT2_PZ_NOT_INITIALIZED: return "Das Feld PrÃ¼fziffer wurde nicht initialisiert";
      case LUT2_NR_NOT_INITIALIZED: return "Das Feld NR wurde nicht initialisiert";
      case LUT2_AENDERUNG_NOT_INITIALIZED: return "Das Feld Ãnderung wurde nicht initialisiert";
      case LUT2_LOESCHUNG_NOT_INITIALIZED: return "Das Feld LÃ¶schung wurde nicht initialisiert";
      case LUT2_NACHFOLGE_BLZ_NOT_INITIALIZED: return "Das Feld Nachfolge-BLZ wurde nicht initialisiert";
      case LUT2_NOT_INITIALIZED: return "die Programmbibliothek wurde noch nicht initialisiert";
      case LUT2_FILIALEN_MISSING: return "der Block mit der Filialenanzahl fehlt in der LUT-Datei";
      case LUT2_PARTIAL_OK: return "es wurden nicht alle Blocks geladen";
      case LUT2_Z_BUF_ERROR: return "Buffer error in den ZLIB Routinen";
      case LUT2_Z_MEM_ERROR: return "Memory error in den ZLIB-Routinen";
      case LUT2_Z_DATA_ERROR: return "Datenfehler im komprimierten LUT-Block";
      case LUT2_BLOCK_NOT_IN_FILE: return "Der Block ist nicht in der LUT-Datei enthalten";
      case LUT2_DECOMPRESS_ERROR: return "Fehler beim Dekomprimieren eines LUT-Blocks";
      case LUT2_COMPRESS_ERROR: return "Fehler beim Komprimieren eines LUT-Blocks";
      case LUT2_FILE_CORRUPTED: return "Die LUT-Datei ist korrumpiert";
      case LUT2_NO_SLOT_FREE: return "Im Inhaltsverzeichnis der LUT-Datei ist kein Slot mehr frei";
      case UNDEFINED_SUBMETHOD: return "Die (Unter)Methode ist nicht definiert";
      case EXCLUDED_AT_COMPILETIME: return "Der benÃ¶tigte Programmteil wurde beim Kompilieren deaktiviert";
      case INVALID_LUT_VERSION: return "Die Versionsnummer fÃ¼r die LUT-Datei ist ungÃ¼ltig";
      case INVALID_PARAMETER_STELLE1: return "ungÃ¼ltiger PrÃ¼fparameter (erste zu prÃ¼fende Stelle)";
      case INVALID_PARAMETER_COUNT: return "ungÃ¼ltiger PrÃ¼fparameter (Anzahl zu prÃ¼fender Stellen)";
      case INVALID_PARAMETER_PRUEFZIFFER: return "ungÃ¼ltiger PrÃ¼fparameter (Position der PrÃ¼fziffer)";
      case INVALID_PARAMETER_WICHTUNG: return "ungÃ¼ltiger PrÃ¼fparameter (Wichtung)";
      case INVALID_PARAMETER_METHODE: return "ungÃ¼ltiger PrÃ¼fparameter (Rechenmethode)";
      case LIBRARY_INIT_ERROR: return "Problem beim Initialisieren der globalen Variablen";
      case LUT_CRC_ERROR: return "PrÃ¼fsummenfehler in der blz.lut Datei";
      case FALSE_GELOESCHT: return "falsch (die BLZ wurde auÃerdem gelÃ¶scht)";
      case OK_NO_CHK_GELOESCHT: return "ok, ohne PrÃ¼fung (die BLZ wurde allerdings gelÃ¶scht)";
      case OK_GELOESCHT: return "ok (die BLZ wurde allerdings gelÃ¶scht)";
      case BLZ_GELOESCHT: return "die Bankleitzahl wurde gelÃ¶scht";
      case INVALID_BLZ_FILE: return "Fehler in der blz.txt Datei (falsche ZeilenlÃ¤nge)";
      case LIBRARY_IS_NOT_THREAD_SAFE: return "undefinierte Funktion; die library wurde mit THREAD_SAFE=0 kompiliert";
      case FATAL_ERROR: return "schwerer Fehler im Konto_check-Modul";
      case INVALID_KTO_LENGTH: return "ein Konto muÃ zwischen 1 und 10 Stellen haben";
      case FILE_WRITE_ERROR: return "kann Datei nicht schreiben";
      case FILE_READ_ERROR: return "kann Datei nicht lesen";
      case ERROR_MALLOC: return "kann keinen Speicher allokieren";
      case NO_BLZ_FILE: return "die blz.txt Datei wurde nicht gefunden";
      case INVALID_LUT_FILE: return "die blz.lut Datei ist inkosistent/ungÃ¼ltig";
      case NO_LUT_FILE: return "die blz.lut Datei wurde nicht gefunden";
      case INVALID_BLZ_LENGTH: return "die Bankleitzahl ist nicht achtstellig";
      case INVALID_BLZ: return "die Bankleitzahl ist ungÃ¼ltig";
      case INVALID_KTO: return "das Konto ist ungÃ¼ltig";
      case NOT_IMPLEMENTED: return "die Methode wurde noch nicht implementiert";
      case NOT_DEFINED: return "die Methode ist nicht definiert";
      case FALSE: return "falsch";
      case OK: return "ok";
      case EE: if(eep)return (char *)eep; else return "";
      case OK_NO_CHK: return "ok, ohne PrÃ¼fung";
      case OK_TEST_BLZ_USED: return "ok; fÃ¼r den Test wurde eine Test-BLZ verwendet";
      case LUT2_VALID: return "Der Datensatz ist aktuell gÃ¼ltig";
      case LUT2_NO_VALID_DATE: return "Der Datensatz enthÃ¤lt kein GÃ¼ltigkeitsdatum";
      case LUT1_SET_LOADED: return "Die Datei ist im alten LUT-Format (1.0/1.1)";
      case LUT1_FILE_GENERATED: return "ok; es wurde allerdings eine LUT-Datei im alten Format (1.0/1.1) generiert";
      default: return "ungÃ¼ltiger RÃ¼ckgabewert";
   }
}

/* Funktion kto_check_retval2txt_short() +§§§1 */
/* ###########################################################################
 * # Die Funktion kto_check_retval2txt_short() wandelt die numerischen       #
 * # Rückgabwerte in kurze Klartexte (symbolische Konstanten) um.            #
 * #                                                                         #
 * # Copyright (C) 2007 Michael Plugge <m.plugge@hs-mannheim.de>             #
 * ###########################################################################
 */

DLL_EXPORT char *kto_check_retval2txt_short(int retval)
{
   switch(retval){
      case INVALID_SEARCH_RANGE: return "INVALID_SEARCH_RANGE";
      case KEY_NOT_FOUND: return "KEY_NOT_FOUND";
      case BAV_FALSE: return "BAV_FALSE";
      case LUT2_NO_USER_BLOCK: return "LUT2_NO_USER_BLOCK";
      case INVALID_SET: return "INVALID_SET";
      case NO_GERMAN_BIC: return "NO_GERMAN_BIC";
      case IPI_CHECK_INVALID_LENGTH: return "IPI_CHECK_INVALID_LENGTH";
      case IPI_INVALID_CHARACTER: return "IPI_INVALID_CHARACTER";
      case IPI_INVALID_LENGTH: return "IPI_INVALID_LENGTH";
      case LUT1_FILE_USED: return "LUT1_FILE_USED";
      case MISSING_PARAMETER: return "MISSING_PARAMETER";
      case IBAN2BIC_ONLY_GERMAN: return "IBAN2BIC_ONLY_GERMAN";
      case IBAN_OK_KTO_NOT: return "IBAN_OK_KTO_NOT";
      case KTO_OK_IBAN_NOT: return "KTO_OK_IBAN_NOT";
      case TOO_MANY_SLOTS: return "TOO_MANY_SLOTS";
      case INIT_FATAL_ERROR: return "INIT_FATAL_ERROR";
      case INCREMENTAL_INIT_NEEDS_INFO: return "INCREMENTAL_INIT_NEEDS_INFO";
      case INCREMENTAL_INIT_FROM_DIFFERENT_FILE: return "INCREMENTAL_INIT_FROM_DIFFERENT_FILE";
      case DEBUG_ONLY_FUNCTION: return "DEBUG_ONLY_FUNCTION";
      case LUT2_INVALID: return "LUT2_INVALID";
      case LUT2_NOT_YET_VALID: return "LUT2_NOT_YET_VALID";
      case LUT2_NO_LONGER_VALID: return "LUT2_NO_LONGER_VALID";
      case LUT2_GUELTIGKEIT_SWAPPED: return "LUT2_GUELTIGKEIT_SWAPPED";
      case LUT2_INVALID_GUELTIGKEIT: return "LUT2_INVALID_GUELTIGKEIT";
      case LUT2_INDEX_OUT_OF_RANGE: return "LUT2_INDEX_OUT_OF_RANGE";
      case LUT2_INIT_IN_PROGRESS: return "LUT2_INIT_IN_PROGRESS";
      case LUT2_BLZ_NOT_INITIALIZED: return "LUT2_BLZ_NOT_INITIALIZED";
      case LUT2_FILIALEN_NOT_INITIALIZED: return "LUT2_FILIALEN_NOT_INITIALIZED";
      case LUT2_NAME_NOT_INITIALIZED: return "LUT2_NAME_NOT_INITIALIZED";
      case LUT2_PLZ_NOT_INITIALIZED: return "LUT2_PLZ_NOT_INITIALIZED";
      case LUT2_ORT_NOT_INITIALIZED: return "LUT2_ORT_NOT_INITIALIZED";
      case LUT2_NAME_KURZ_NOT_INITIALIZED: return "LUT2_NAME_KURZ_NOT_INITIALIZED";
      case LUT2_PAN_NOT_INITIALIZED: return "LUT2_PAN_NOT_INITIALIZED";
      case LUT2_BIC_NOT_INITIALIZED: return "LUT2_BIC_NOT_INITIALIZED";
      case LUT2_PZ_NOT_INITIALIZED: return "LUT2_PZ_NOT_INITIALIZED";
      case LUT2_NR_NOT_INITIALIZED: return "LUT2_NR_NOT_INITIALIZED";
      case LUT2_AENDERUNG_NOT_INITIALIZED: return "LUT2_AENDERUNG_NOT_INITIALIZED";
      case LUT2_LOESCHUNG_NOT_INITIALIZED: return "LUT2_LOESCHUNG_NOT_INITIALIZED";
      case LUT2_NACHFOLGE_BLZ_NOT_INITIALIZED: return "LUT2_NACHFOLGE_BLZ_NOT_INITIALIZED";
      case LUT2_NOT_INITIALIZED: return "LUT2_NOT_INITIALIZED";
      case LUT2_FILIALEN_MISSING: return "LUT2_FILIALEN_MISSING";
      case LUT2_PARTIAL_OK: return "LUT2_PARTIAL_OK";
      case LUT2_Z_BUF_ERROR: return "LUT2_Z_BUF_ERROR";
      case LUT2_Z_MEM_ERROR: return "LUT2_Z_MEM_ERROR";
      case LUT2_Z_DATA_ERROR: return "LUT2_Z_DATA_ERROR";
      case LUT2_BLOCK_NOT_IN_FILE: return "LUT2_BLOCK_NOT_IN_FILE";
      case LUT2_DECOMPRESS_ERROR: return "LUT2_DECOMPRESS_ERROR";
      case LUT2_COMPRESS_ERROR: return "LUT2_COMPRESS_ERROR";
      case LUT2_FILE_CORRUPTED: return "LUT2_FILE_CORRUPTED";
      case LUT2_NO_SLOT_FREE: return "LUT2_NO_SLOT_FREE";
      case UNDEFINED_SUBMETHOD: return "UNDEFINED_SUBMETHOD";
      case EXCLUDED_AT_COMPILETIME: return "EXCLUDED_AT_COMPILETIME";
      case INVALID_LUT_VERSION: return "INVALID_LUT_VERSION";
      case INVALID_PARAMETER_STELLE1: return "INVALID_PARAMETER_STELLE1";
      case INVALID_PARAMETER_COUNT: return "INVALID_PARAMETER_COUNT";
      case INVALID_PARAMETER_PRUEFZIFFER: return "INVALID_PARAMETER_PRUEFZIFFER";
      case INVALID_PARAMETER_WICHTUNG: return "INVALID_PARAMETER_WICHTUNG";
      case INVALID_PARAMETER_METHODE: return "INVALID_PARAMETER_METHODE";
      case LIBRARY_INIT_ERROR: return "LIBRARY_INIT_ERROR";
      case LUT_CRC_ERROR: return "LUT_CRC_ERROR";
      case FALSE_GELOESCHT: return "FALSE_GELOESCHT";
      case OK_NO_CHK_GELOESCHT: return "OK_NO_CHK_GELOESCHT";
      case OK_GELOESCHT: return "OK_GELOESCHT";
      case BLZ_GELOESCHT: return "BLZ_GELOESCHT";
      case INVALID_BLZ_FILE: return "INVALID_BLZ_FILE";
      case LIBRARY_IS_NOT_THREAD_SAFE: return "LIBRARY_IS_NOT_THREAD_SAFE";
      case FATAL_ERROR: return "FATAL_ERROR";
      case INVALID_KTO_LENGTH: return "INVALID_KTO_LENGTH";
      case FILE_WRITE_ERROR: return "FILE_WRITE_ERROR";
      case FILE_READ_ERROR: return "FILE_READ_ERROR";
      case ERROR_MALLOC: return "ERROR_MALLOC";
      case NO_BLZ_FILE: return "NO_BLZ_FILE";
      case INVALID_LUT_FILE: return "INVALID_LUT_FILE";
      case NO_LUT_FILE: return "NO_LUT_FILE";
      case INVALID_BLZ_LENGTH: return "INVALID_BLZ_LENGTH";
      case INVALID_BLZ: return "INVALID_BLZ";
      case INVALID_KTO: return "INVALID_KTO";
      case NOT_IMPLEMENTED: return "NOT_IMPLEMENTED";
      case NOT_DEFINED: return "NOT_DEFINED";
      case FALSE: return "FALSE";
      case OK: return "OK";
      case EE: return "EE";
      case OK_NO_CHK: return "OK_NO_CHK";
      case OK_TEST_BLZ_USED: return "OK_TEST_BLZ_USED";
      case LUT2_VALID: return "LUT2_VALID";
      case LUT2_NO_VALID_DATE: return "LUT2_NO_VALID_DATE";
      case LUT1_SET_LOADED: return "LUT1_SET_LOADED";
      case LUT1_FILE_GENERATED: return "LUT1_FILE_GENERATED";
      default: return "UNDEFINED_RETVAL";
   }
}


/* Funktion get_lut_info_b() +§§§1 */
/*
 * ######################################################################
 * # get_lut_info_b(): Infozeile der LUT-Datei holen, VB-Version        #
 * #                                                                    #
 * # Dies ist die Visual-Basic Version von get_lut_info() (s.u.). Die   #
 * # Funktion benutzt System.Text.StringBuilder, um einen Speicher-     #
 * # bereich in VB zu allokieren; dieser wird dann an C übergeben, der  #
 * # info-String kann einfach kopiert werden und der für prolog         #
 * # allokierte Speicher kann (hier) gleich wieder freigegeben werden.  #
 * ######################################################################
 */

DLL_EXPORT int get_lut_info_b(char **info_p,char *lutname)
{
   char *prolog,*info;
   int retval;

   if((retval=get_lut_info2(lutname,NULL,&prolog,&info,NULL))!=OK)return retval;
   if(info)
      strncpy(*info_p,info,1024);
   else
      **info_p=0;
   FREE(prolog);
   return OK;
}

DLL_EXPORT int get_lut_info2_b(char *lutname,int *version,char **prolog_p,char **info_p,char **user_info_p)
{
   char *prolog,*info,*user_info;
   int retval;

   if((retval=get_lut_info2(lutname,version,&prolog,&info,&user_info))!=OK)return retval;
   if(prolog){
      strncpy(*prolog_p,prolog,1024);
      FREE(prolog);
   }
   else
      **prolog_p=0;
   if(info){
      strncpy(*info_p,info,1024);
      FREE(info);
   }
   else
      **info_p=0;
   if(user_info){
      strncpy(*user_info_p,user_info,1024);
      FREE(user_info);
   }
   else
      **user_info_p=0;
   FREE(prolog);
   return OK;
}

/* Funktion get_lut_info() +§§§1 */
/*
 * ######################################################################
 * # get_lut_info(): Infozeile der LUT-Datei holen                      #
 * #                                                                    #
 * # Die Funktion holt die Infozeile(n) der LUT-Datei in einen          #
 * # statischen Speicherbereich und setzt die Variable info auf diesen  #
 * # Speicher. Diese Funktion wird erst ab Version 1.1 der LUT-Datei    #
 * # unterstützt.                                                       #
 * #                                                                    #
 * # Parameter:                                                         #
 * #    info:     Die Variable wird auf die Infozeile gesetzt           #
 * #    lut_name: Name der LUT-Datei                                    #
 * #                                                                    #
 * # Rückgabewerte:                                                     #
 * #    ERROR_MALLOC       kann keinen Speicher allokieren              #
 * #    NO_LUT_FILE        LUT-Datei nicht gefunden (Pfad falsch?)      #
 * #    FATAL_ERROR        kann die LUT-Datei nicht lesen               #
 * #    INVALID_LUT_FILE   Fehler in der LUT-Datei (Format, CRC...)     #
 * #    OK                 Erfolg                                       #
 * #                                                                    #
 * # Copyright (C) 2007 Michael Plugge <m.plugge@hs-mannheim.de>        #
 * ######################################################################
 */

DLL_EXPORT int get_lut_info_t(char **info,char *lut_name,KTO_CHK_CTX *ctx)
{
   return get_lut_info(info,lut_name);
}

DLL_EXPORT int get_lut_info(char **info,char *lut_name)
{
   char *prolog,*ptr;
   int retval;

   if((retval=get_lut_info2(lut_name,NULL,&prolog,&ptr,NULL))!=OK)return retval;
   if(ptr){
      *info=malloc(strlen(ptr)+1);
      strcpy(*info,ptr);   /* Infozeile kopieren, damit prolog freigegeben werden kann */
   }
   else
      *info=NULL;
   FREE(prolog);
   return OK;
}

/* Funktion cleanup_kto() +§§§1 */
/* ###########################################################################
 * # cleanup_kto(): Speicher freigeben                                       #
 * # Diese Funktion ist Teil des alten Interfaces und wurde als Wrapper      #
 * # zu den neuen Routinen umgestellt.                                       #
 * #                                                                         #
 * # Copyright (C) 2007 Michael Plugge <m.plugge@hs-mannheim.de>             #
 * ###########################################################################
 */

DLL_EXPORT int cleanup_kto_t(KTO_CHK_CTX *ctx)
{
   return cleanup_kto();
}

DLL_EXPORT int cleanup_kto(void)
{
   return lut_cleanup();
}

/* Funktion get_kto_check_version() +§§§1 */
/* ###########################################################################
 * #  Diese Funktion gibt die Version und das Datum der Kompilierung der     #
 * #  konto_check library als String zurück.                                .#
 * #                                                                         #
 * # Copyright (C) 2007 Michael Plugge <m.plugge@hs-mannheim.de>             #
 * ###########################################################################
 */

DLL_EXPORT char *get_kto_check_version(void)
{
   return "konto_check Version " VERSION " vom " VERSION_DATE " (kompiliert " __DATE__ ", " __TIME__ ")";
}


/* Funktion dump_lutfile_p() +§§§1 */
/* ###########################################################################
 * # Diese Funktion dient dazu, Felder der **geladenen** LUT-Datei (in       #
 * # beliebiger Reihenfolge) auszugeben. Die auszugebenden Felder werden in  #
 * # dem Array required spezifiziert. Es werden nur Felder der Deutschen     #
 * # Bundesbank berücksichtigt; andere Felder werden ignoriert. Die Funktion #
 * # benutzt statt des Integerarrays einen Integerparameter, da diese        # 
 * # Variante vor allem für Perl gedacht ist; in der Umgebung ist es         #
 * # etwas komplizierter, ein Array zu übergeben.                            #
 * #                                                                         #
 * # Copyright (C) 2008 Michael Plugge <m.plugge@hs-mannheim.de>             #
 * ###########################################################################
 */

DLL_EXPORT int dump_lutfile_p(char *outputname,UINT4 felder)
{
   UINT4 *felder1;

   switch(felder){
      case 0:  felder1=(UINT4 *)lut_set_o0; break;  
      case 1:  felder1=(UINT4 *)lut_set_o1; break;
      case 2:  felder1=(UINT4 *)lut_set_o2; break;
      case 3:  felder1=(UINT4 *)lut_set_o3; break;
      case 4:  felder1=(UINT4 *)lut_set_o4; break;
      case 5:  felder1=(UINT4 *)lut_set_o5; break;
      case 6:  felder1=(UINT4 *)lut_set_o6; break;
      case 7:  felder1=(UINT4 *)lut_set_o7; break;
      case 8:  felder1=(UINT4 *)lut_set_o8; break;
      case 9:  felder1=(UINT4 *)lut_set_o9; break;
      default: felder1=(UINT4 *)lut_set_o9; break;
   }
   return dump_lutfile(outputname,felder1);
}

/* Funktion dump_lutfile() +§§§1 */
/* ###########################################################################
 * # Diese Funktion dient dazu, Felder der **geladenen** LUT-Datei (in       #
 * # beliebiger Reihenfolge) auszugeben. Die auszugebenden Felder werden in  #
 * # dem Array required spezifiziert. Es werden nur Felder der Deutschen     #
 * # Bundesbank berücksichtigt; andere Felder werden ignoriert.              #
 * #                                                                         #
 * # Copyright (C) 2007 Michael Plugge <m.plugge@hs-mannheim.de>             #
 * ###########################################################################
 */

DLL_EXPORT int dump_lutfile(char *outputname,UINT4 *required)
{
   int i,i1,j=0,k,cnt,m,zeige_filialen;
   UINT4 xr[MAX_SLOTS];
   FILE *out;

   if(!blz)return LUT2_BLZ_NOT_INITIALIZED;
   if(!pz_methoden)return LUT2_PZ_NOT_INITIALIZED;
   zeige_filialen=0;
   if(!outputname)
      out=stderr;
   else if(!(out=fopen(outputname,"w")))
      return FILE_WRITE_ERROR;
   if(!required)
      for(required=xr,i=0;i<MAX_SLOTS;i++)xr[i]=i+1; /* Default: alle Blocks ausgeben */
   else{
      for(i=j=0;i<MAX_SLOTS && j<MAX_SLOTS && required[i];i++)if(required[i]==LUT2_NAME_NAME_KURZ){
         xr[j++]=LUT2_NAME;
         xr[j++]=LUT2_NAME_KURZ;
      }
      else if(i>0 && required[i]==LUT2_FILIALEN && required[i-1]==LUT2_FILIALEN)
         zeige_filialen=1;
      else
         xr[j++]=required[i];
   }
   xr[j]=0;
   if(current_info)
      fprintf(out,"Infoblock der Daten:\n====================\n%s\n",current_info);
   else if(lut_prolog)
      fprintf(out,"Prolog:\n=======\n%s\n",lut_prolog);
   for(i=cnt=0;(xr[cnt]);cnt++)switch(xr[cnt]){  /* Anzahl der Elemente im Array bestimmen und Überschriften ausgeben */
      case 1:
         fprintf(out,"%-8s ","BLZ");
         i+=9;
         break;
      case 2:
         fprintf(out,"%6s ","Filia.");
         i+=7;
         break;
      case 3:
         fprintf(out,"%-58s ","Bankname");
         i+=59;
         break;
      case 4:
         fprintf(out,"%-5s ","Plz");
         i+=6;
         break;
      case 5:
         fprintf(out,"%-35s ","Ort");
         i+=36;
         break;
      case 6:
         fprintf(out,"%-27s ","Bankname (kurz)");
         i+=28;
         break;
      case 7:
         fprintf(out,"%5s ","PAN");
         i+=6;
         break;
      case 8:
         fprintf(out,"%11s ","BIC");
         i+=12;
         break;
      case 9:
         fprintf(out,"%2s ","PZ");
         i+=3;
         break;
      case 10:
         fprintf(out,"%6s ","LfdNr");
         i+=7;
         break;
      case 11:
         fprintf(out,"%1s ","Ä");
         i+=2;
         break;
      case 12:
         fprintf(out,"%1s ","L");
         i+=2;
         break;
      case 13:
         fprintf(out,"%8s ","NachfBLZ");
         i+=9;
         break;
      case 14:
         fprintf(out,"%-58s ","Name, Kurzn.");
         i+=59;
         break;
      case 15:
         fprintf(out,"%1s ","Infoblock");
         i+=2;
         break;
      default:
         break;
   }
   fputc('\n',out);
   while(--i)fputc('=',out);
   fputc('\n',out);

   for(i=0;i<lut2_cnt_hs;i++){
      if(filialen && zeige_filialen)
         k=startidx[i]+filialen[i]-1;
      else
         k=startidx[i];
      for(i1=startidx[i];i1<=k;i1++){
         for(j=0;j<cnt;j++)switch(xr[j]){
            case LUT2_BLZ: 
               if(blz)fprintf(out,"%8d ",blz[i]);
               break;
            case LUT2_FILIALEN: 
               if(filialen){
                  if(i1==startidx[i]){
                     if(filialen[i]==1)
                        fprintf(out,"H      ");
                     else if(filialen[i]<11)
                        fprintf(out,"H  [%1d] ",filialen[i]-1);
                     else if(filialen[i]<101)
                        fprintf(out,"H [%2d] ",filialen[i]-1);
                     else
                        fprintf(out,"H[%3d] ",filialen[i]-1);
                  }
                  else
                     fprintf(out,"F %3d  ",i1-startidx[i]);
               }
               break;
            case LUT2_NAME: 
               if(name)fprintf(out,"%-58s ",name[i1]);
               break;
            case LUT2_PLZ: 
               if(plz)fprintf(out,"%5d ",plz[i1]);
               break;
            case LUT2_ORT: 
               if(ort)fprintf(out,"%-35s ",ort[i1]);
               break;
            case LUT2_NAME_KURZ: 
               if(name_kurz)fprintf(out,"%-27s ",name_kurz[i1]);
               break;
            case LUT2_PAN: 
               if(pan)fprintf(out,"%5d ",pan[i1]);
               break;
            case LUT2_BIC: 
               if(bic)fprintf(out,"%11s ",bic[i1]);
               break;
            case LUT2_PZ: 
               if(pz_methoden){
                  m=pz_methoden[i];
                  if(m<100)
                     fprintf(out,"%02d ",m);
                  else
                     fprintf(out,"%c%d ",m/10-10+'A',m%10);
               }
               break;
            case LUT2_NR: 
               if(bank_nr)fprintf(out,"%6d ",bank_nr[i1]);
               break;
            case LUT2_AENDERUNG: 
               if(aenderung)fprintf(out,"%c ",aenderung[i1]);
               break;
            case LUT2_LOESCHUNG: 
               if(loeschung)fprintf(out,"%c ",loeschung[i1]);
               break;
            case LUT2_NACHFOLGE_BLZ: 
               if(nachfolge_blz)fprintf(out,"%8d ",nachfolge_blz[i1]);
               break;
            default:
               break;
         }
         fputc('\n',out);
      }
   }
   if(outputname)fclose(out);
   return OK;
}

/* Funktion rebuild_blzfile() +§§§1 */
/* ###########################################################################
 * # Die Funktion rebuild_blzfile() ist ein Härtetest für die LUT2-Routinen: #
 * # die BLZ-Datei wird komplett aus einer LUT-Datei neu generiert.          #
 * # Es ist allerdings zu beachten, daß in der BLZ-Datei die Hauptstellen    #
 * # oft erst nach den Zweigstellen kommen, während sie in der LUT-Datei     #
 * # vor die Zweigstellen sortiert werden; außerdem werden die Zweigstellen  #
 * # in der LUT-Datei nach Postleitzahlen sortiert. Eine sortierte Version   #
 * # beider Dateien zeigt jedoch keine Unterschiede mehr.                    #
 * #                                                                         #
 * # Falls der Parameter set 1 oder 2 ist, wird als Eingabedatei eine LUT-   #
 * # datei erwartet; bei einem set-Parameter von 0 eine Klartextdatei        #
 * # (Bundesbankdatei).                                                      #
 * #                                                                         #
 * # Copyright (C) 2007,2009 Michael Plugge <m.plugge@hs-mannheim.de>        #
 * ###########################################################################
 */

DLL_EXPORT int rebuild_blzfile(char *inputname,char *outputname,UINT4 set)
{
   char pbuf[256],*b,pan_buf[8],nr_buf[8],tmpfile[16];
   char  **p_name,**p_name_kurz,**p_ort,**p_bic,*p_aenderung,*p_loeschung;
   int i,j,ret;
   int cnt,*p_nachfolge_blz,id,p_pz,*p_nr,*p_plz,*p_pan;
   UINT4 lut_set[30];
   FILE *out;
   struct stat s_buf;

   if(!set){ /* set-Parameter 0: BLZ-Datei (Klartext) als Eingabedatei, LUT-Datei generieren */

         /* eigene Version von mktemp, da die Libraryversion immer einen Linkerfehler
          * erzeugt, der sich nicht deaktivieren läßt (ist hier auch nicht kritisch)
          */
      for(i=0;i<100000;i++){
         sprintf(tmpfile,"blz_tmp.%05d",i);
         if((stat(tmpfile,&s_buf)==-1) && (errno==EBADF || errno==ENOENT))break;
      }
      lut_set[0]=LUT2_BLZ;
      lut_set[1]=LUT2_PZ;
      lut_set[2]=LUT2_FILIALEN;
      for(i=0;(lut_set[i+3]=lut_set_9[i]) && i<28;i++);
      lut_set[i+3]=0;
      if(i==100000)return FATAL_ERROR; /* keine mögliche Ausgabedatei gefunden */
      ret=generate_lut2(inputname,tmpfile,"Testdatei für LUT2",NULL,lut_set,20,3,0);
      printf("generate_lut2: %s\n",kto_check_retval2txt_short(ret));
      if(ret!=OK){
         unlink(tmpfile);
         return ret;
      }
      ret=kto_check_init_p(tmpfile,9,0,0);
      printf("init(): %s\n",kto_check_retval2txt_short(ret));
      unlink(tmpfile);
      if(ret!=OK)return ret;
   }
   else  /* set-Parameter 1 oder 2: LUT-Datei als Eingabedatei */
      if((ret=kto_check_init_p(inputname,9,set,0))!=OK)return ret;

   if(!(out=fopen(outputname,"w")))return FILE_WRITE_ERROR;
   for(i=0;i<lut2_cnt_hs;i++){
      sprintf(b=pbuf,"%d",blz[i]);
      lut_multiple(pbuf,&cnt,NULL,&p_name,&p_name_kurz,&p_plz,&p_ort,&p_pan,&p_bic,
            &p_pz,&p_nr,&p_aenderung,&p_loeschung,&p_nachfolge_blz,&id,NULL,NULL);
      if(*p_pan)
         sprintf(pan_buf,"%05d",*p_pan);
      else
         *pan_buf=0;
      if(*p_nr)
         sprintf(nr_buf,"%06d",*p_nr);
      else
         *nr_buf=0;
      fprintf(out,"%8s1%-58s%05d%-35s%-27s%5s%-11s%X%d%6s%c%c%08d\n",
            pbuf,*p_name,*p_plz,*p_ort,*p_name_kurz,pan_buf,*p_bic,p_pz/10,p_pz%10,
            nr_buf,*p_aenderung,*p_loeschung,*p_nachfolge_blz);
      for(j=1;j<cnt;j++){
         if(p_pan[j])
            sprintf(pan_buf,"%05d",p_pan[j]);
         else
            *pan_buf=0;
         if(p_nr[j])
            sprintf(nr_buf,"%06d",p_nr[j]);
         else
            *nr_buf=0;
         fprintf(out,"%8s2%-58s%05d%-35s%-27s%5s%-11s%X%d%6s%c%c%08d\n",
               pbuf,p_name[j],p_plz[j],p_ort[j],p_name_kurz[j],pan_buf,p_bic[j],
               p_pz/10,p_pz%10,nr_buf,p_aenderung[j],p_loeschung[j],p_nachfolge_blz[j]);
      }               
   }
   return OK;
}

/* Funktion iban2bic() +§§§1 */
/* ###########################################################################
 * # Die Funktion iban2bic extrahiert aus einer IBAN (International Bank     #
 * # Account Number) Kontonummer und Bankleitzahl, und bestimmt zu der BLZ   #
 * # die zugehörige BIC. Voraussetzung ist natürlich, daß das BIC Feld in    #
 * # der geladenen LUT-Datei enthalten ist. BLZ und Kontonummer werden,      #
 * # falls gewünscht, in zwei Variablen zurückgegeben.                       #
 * #                                                                         #
 * # Die Funktion arbeitet nur für deutsche Banken, da für andere keine      #
 * # Infos vorliegen.                                                        #
 * #                                                                         #
 * # Parameter:                                                              #
 * #    iban:       die IBAN, zu der die Werte bestimmt werden sollen        #
 * #    retval:     NULL oder Adresse einer Variablen, in die der Rückgabe-  #
 * #                wert der Umwandlung geschrieben wird                     #
 * #    blz:        NULL, oder Adresse eines Speicherbereichs mit mindestens #
 * #                9 Byte, in den die BLZ geschrieben wird                  #
 * #    kto:        NULL, oder Adresse eines Speicherbereichs mit mindestens #
 * #                11 Byte, in den die Kontonummer geschrieben wird.        #
 * #                                                                         #
 * # Rückgabe:      der zu der übergebenen IBAN gehörende BIC                #
 * #                                                                         #
 * # Copyright (C) 2008 Michael Plugge <m.plugge@hs-mannheim.de>             #
 * ###########################################################################
 */

DLL_EXPORT char *iban2bic(char *iban,int *retval,char *blz,char *kto)
{
   char check[16],*ptr,*dptr;
   int i;

   if(tolower(*iban)!='d' || tolower(*(iban+1))!='e'){
      if(retval)*retval=IBAN2BIC_ONLY_GERMAN;
      if(blz)*blz=0;
      if(kto)*kto=0;
      return "";
   }
   for(ptr=iban+4,dptr=check,i=0;i<8;ptr++)if(isdigit(*ptr)){
      *dptr++=*ptr;
      i++;
   }
   *dptr=0;
   if(blz){
      for(ptr=iban+4,dptr=blz,i=0;i<8;ptr++)if(isdigit(*ptr)){
         *dptr++=*ptr;
         i++;
      }
      *dptr=0;
   }
   if(kto){
      for(dptr=kto,i=0;i<10;ptr++)if(isdigit(*ptr)){
         *dptr++=*ptr;
         i++;
      }
      *dptr=0;
   }
   return lut_bic(check,0,retval);
}

/* Funktion iban_gen() +§§§1 */
/* ###########################################################################
 * # Die Funktion iban_gen generiert aus Bankleitzahl und Kontonummer eine   #
 * # IBAN (International Bank Account Number). Die Funktion ist lediglich    #
 * # zum Test der anderen IBAN-Routinen geschrieben, und sollte nicht zum    #
 * # Generieren realer IBANs benutzt werden (s.u.).                          #
 * #                                                                         #
 * # Parameter:                                                              #
 * #    blz:        Bankleitzahl. Falls der Bankleitzahl ein + vorangestellt #
 * #                wird, wird die entsprechende Bankverbindung nicht auf    #
 * #                Korrektheit getestet (ansonsten schon; dies ist zum      #
 * #                Debuggen der konto_check Bibliothek gedacht).            #
 * #    kto:        Kontonummer                                              #
 * #    retval:     NULL oder Adresse einer Variablen, in die der Rückgabe-  #
 * #                wert der Kontoprüfung geschrieben wird                   #
 * #                                                                         #
 * # Rückgabe:      die erzeugte IBAN. Für die Rückgabe wird Speicher        #
 * #                allokiert; dieser muß nach der Benutzung wieder frei-    #
 * #                gegeben werden. Falls der Test der Bankverbindung        #
 * #                fehlschlägt, wird der entsprechende Fehlercode in die    #
 * #                Variable retval geschrieben und NULL zurückgegeben.      #
 * #                                                                         #
 * # Copyright (C) 2008 Michael Plugge <m.plugge@hs-mannheim.de>             #
 * ###########################################################################
 */

/* ####################################################################################
 * # hier erstmal eine Warnung vorweg:                                                #
 * #                                                                                  #
 * # Bitte beachten Sie: Die Erzeugung einer IBAN für den realen Einsatz sollte       #
 * # unbedingt durch eine Bank erfolgen. Selber erzeugte IBANs können zwar            #
 * # syntaktisch richtig sein, aber nicht den Aufbauvorschriften der jeweiligen       #
 * # Bank entsprechen. Dies kann zu manuellem Korrekturaufwand seitens der            #
 * # beauftragten Bank und höheren Entgelten für den Zahlungsauftrag führen! Alle     #
 * # deutschen Banken geben den persönlichen IBAN und den BIC auf dem Kontoauszug     #
 * # an. Für selbst veranlaßte Überweisungen sind BIC und IBAN vom                    #
 * # Zahlungsempfänger zu erfragen, bitte aus o.g. Gründen diese nicht selber         #
 * # berechnen!                                                                       #
 * ####################################################################################
 */

DLL_EXPORT char *iban_gen(char *blz,char *kto,int *retval)
{
   char c,check[128],iban[128],*ptr,*dptr;
   int j,ret;
   UINT4 zahl,rest;

      /* erstmal das Konto testen */
   if(*blz=='+')  /* kein Test */
      blz++;
   else if((ret=kto_check_blz(blz,kto))<=0){   /* Konto fehlerhaft */
      if(retval)*retval=ret;
      return NULL;
   }

   sprintf(iban,"DE00%8s%10s",blz,kto);
   for(ptr=iban;*ptr;ptr++)if(*ptr==' ')*ptr='0';
   for(ptr=iban+4,dptr=check;*ptr;ptr++){
      if((c=*ptr)>='0' && c<='9')
         *dptr++=c;
      else if(c>='A' && c<='Z'){
         c+=10-'A';
         *dptr++=c/10+'0';
         *dptr++=c%10+'0';
      }
      else if(c>='a' && c<='z'){
         c+=10-'a';
         *dptr++=c/10+'0';
         *dptr++=c%10+'0';
      }
   }

      /* Ländercode (2-stellig/alphabetisch) kopieren */
   ptr=iban;
   if((c=*ptr++)>='A' && c<='Z'){
      c+=10-'A';
      *dptr++=c/10+'0';
      *dptr++=c%10+'0';
   }
   else if(c>='a' && c<='z'){
      c+=10-'a';
      *dptr++=c/10+'0';
      *dptr++=c%10+'0';
   }
   if((c=*ptr++)>='A' && c<='Z'){
      c+=10-'A';
      *dptr++=c/10+'0';
      *dptr++=c%10+'0';
   }
   else if(c>='a' && c<='z'){
      c+=10-'a';
      *dptr++=c/10+'0';
      *dptr++=c%10+'0';
   }

      /* Prüfziffer kopieren */
   *dptr++=*ptr++;
   *dptr++=*ptr++;
   *dptr=0;

   for(rest=0,ptr=check;*ptr;){
      zahl=rest/10;
      zahl=zahl*10+rest%10;
      for(j=0;j<6 && *ptr;ptr++,j++)zahl=zahl*10+ *ptr-'0';
      rest=zahl%97;
   }
   zahl=98-rest;
   *(iban+2)=zahl/10+'0';
   *(iban+3)=zahl%10+'0';
   for(ptr=iban,dptr=check,j=1;*ptr;j++){
      *dptr++=*ptr++;
      if(j>0 && !(j%4))*dptr++=' ';
   }
   *dptr=0;
   ptr=malloc(64);
   strcpy(ptr,check);
   if(retval)*retval=OK;
   return ptr;
}

/* Funktion iban_check() +§§§1 */
/* ###########################################################################
 * # Die Funktion iban_check prüft, ob die Prüfsumme des IBAN ok ist und     #
 * # testet außerdem noch die BLZ/Konto Kombination. Für den Test des Kontos #
 * # wird keine Initialisierung gemacht; diese muß vorher erfolgen.          #
 * #                                                                         #
 * # Parameter:                                                              #
 * #    iban:       IBAN die getestet werden soll                            #
 * #    retval:     NULL oder Adresse einer Variablen, in die der Rückgabe-  #
 * #                wert der Kontoprüfung geschrieben wird                   #
 * #                                                                         #
 * # Copyright (C) 2008 Michael Plugge <m.plugge@hs-mannheim.de>             #
 * ###########################################################################
 */

DLL_EXPORT int iban_check(char *iban,int *retval)
{
   char c,check[128],*ptr,*dptr;
   int j,test,ret;
   UINT4 zahl,rest;

      /* BBAN (Basic Bank Account Number) kopieren (alphanumerisch) */
   test=0;
   for(ptr=iban+4,dptr=check;*ptr;ptr++){
      if((c=*ptr)>='0' && c<='9')
         *dptr++=c;
      else if(c>='A' && c<='Z'){
         c+=10-'A';
         *dptr++=c/10+'0';
         *dptr++=c%10+'0';
      }
      else if(c>='a' && c<='z'){
         c+=10-'a';
         *dptr++=c/10+'0';
         *dptr++=c%10+'0';
      }
   }

      /* Ländercode (2-stellig/alphabetisch) kopieren */
   ptr=iban;
   if((c=*ptr++)>='A' && c<='Z'){
      c+=10-'A';
      *dptr++=c/10+'0';
      *dptr++=c%10+'0';
   }
   else if(c>='a' && c<='z'){
      c+=10-'a';
      *dptr++=c/10+'0';
      *dptr++=c%10+'0';
   }
   if((c=*ptr++)>='A' && c<='Z'){
      c+=10-'A';
      *dptr++=c/10+'0';
      *dptr++=c%10+'0';
   }
   else if(c>='a' && c<='z'){
      c+=10-'a';
      *dptr++=c/10+'0';
      *dptr++=c%10+'0';
   }

      /* Prüfziffer kopieren */
   *dptr++=*ptr++;
   *dptr++=*ptr++;
   *dptr=0;

   for(rest=0,ptr=check;*ptr;){
      zahl=rest/10;
      zahl=zahl*10+rest%10;
      for(j=0;j<6 && *ptr;ptr++,j++)zahl=zahl*10+ *ptr-'0';
      rest=zahl%97;
   }
   zahl=98-rest;
   if(rest==1)test=1;   /* IBAN ok */
   if((*iban=='D' || *iban=='d') && (*(iban+1)=='E' || *(iban+1)=='e')){ /* Konto testen */
      for(ptr=iban+4,dptr=check,j=0;j<8;ptr++)if(isdigit(*ptr)){
         *dptr++=*ptr;
         j++;
      }
      *dptr++=0;
      for(j=0;j<10;ptr++)if(isdigit(*ptr)){
         *dptr++=*ptr;
         j++;
      }
      *dptr=0;
      if((ret=kto_check_blz(check,check+9))>0)test|=2;
      if(retval)*retval=ret;
   }
   else{
      if(test)test|=2;  /* falls IBAN nicht ok ist, test auf 0 lassen */
      if(retval)*retval=NO_GERMAN_BIC;
   }
   switch(test){
      case 1: return IBAN_OK_KTO_NOT;
      case 2: return KTO_OK_IBAN_NOT;
      case 3: return OK;
      default: return FALSE;
   }
}

/* Funktion ipi_gen() +§§§1 */
/* ###########################################################################
 * # Die Funktion ipi_gen generiert einen Strukturierten Verwendungszweck    #
 * # für eine IPI (International Payment Instruction). Der Zweck darf nur    #
 * # Buchstaben und Zahlen enthalten; Buchstaben werden dabei in Großbuch-   #
 * # staben umgewandelt. Andere Zeichen sind hier nicht zulässig. Der        #
 * # Verwendungszweck wird links mit Nullen bis auf 18 Byte aufgefüllt, dann #
 * # die Prüfsumme berechnet und eingesetzt.                                 #
 * #                                                                         #
 * # Parameter:                                                              #
 * #    zweck:      Zweck (maximal 18 Byte)                                  #
 * #    dst:        Adresse eines Speicherbereichs mit mindestens 21 Byte,   #
 * #                in die der generierte Verwendungszweck (mit Prüfsumme)   #
 * #                geschrieben wird, oder NULL (falls nicht benötigt)       #
 * #   papier:      Adresse eines Speicherbereichs mit mindestens 26 Byte,   #
 * #                in die die Papierform des Verwendungszwecks (mit Leer-   #
 * #                zeichen nach jeweils 5 Zeichen) geschrieben wird, oder   #
 * #                NULL (falls nicht benötigt)                              #
 * #                                                                         #
 * # Copyright (C) 2008 Michael Plugge <m.plugge@hs-mannheim.de>             #
 * ###########################################################################
 */

DLL_EXPORT int ipi_gen(char *zweck,char *dst,char *papier)
{
   char c,check[64],buffer[24],*ptr,*dptr;
   int i;
   UINT4 zahl,rest;

   if(dst)*dst=0;
   if(papier)*papier=0;
   if(strlen(zweck)>18)return IPI_INVALID_LENGTH;

      /* der Verwendungszweck wird nun nach dst kopiert und linksbündig mit
       * Nullen aufgefüllt.
       */
   for(ptr=zweck;*ptr;ptr++); /* ptr auf Ende des Verwendungszwecks setzen */
   for(dptr=buffer+20;ptr>=zweck;*dptr--=toupper(*ptr--))if(*ptr && !isalnum(*ptr))return IPI_INVALID_CHARACTER;
   while(dptr>buffer)*dptr--='0';

      /* Verwendungszweck nach check kopieren, dabei Buchstaben konvertieren */
   for(ptr=buffer+2,dptr=check;*ptr;ptr++){
      if((c=*ptr)>='0' && c<='9')
         *dptr++=c;
      else if(c>='A' && c<='Z'){
         c+=10-'A';
         *dptr++=c/10+'0';
         *dptr++=c%10+'0';
      }
      else if(c>='a' && c<='z'){
         c+=10-'a';
         *dptr++=c/10+'0';
         *dptr++=c%10+'0';
      }
   }

      /* Prüfziffer kopieren */
   *dptr++='0';
   *dptr++='0';
   *dptr++=0;

   for(rest=0,ptr=check;*ptr;){
      zahl=rest;
      for(i=0;i<6 && *ptr;ptr++,i++)zahl=zahl*10+ *ptr-'0'; /* maximal sieben weitere Stellen dazu */
      rest=zahl%97;
   }
   zahl=98-rest;

      /* Prüfziffer schreiben */
   *buffer=zahl/10+'0';
   *(buffer+1)=zahl%10+'0';
   if(dst)for(ptr=buffer,dptr=dst;(*dptr++=*ptr++););
   if(papier)for(ptr=buffer,dptr=papier,i=1;(*dptr++=*ptr++);)if(i<20 && !(i++%4))*dptr++=' ';
   return OK;
}

/* Funktion ipi_check() +§§§1 */
/* ###########################################################################
 * # Die Funktion ipi_check testet einen Strukturierten Verwendungszweck     #
 * # für eine IPI (International Payment Instruction). Der Zweck darf nur    #
 * # Buchstaben und Zahlen enthalten und muß genau 20 Byte lang sein, wobei  #
 * # eingestreute Blanks oder Tabs ignoriert werden.                         #
 * #                                                                         #
 * # Parameter:                                                              #
 * #    zweck: der Strukturierte Verwendungszweck, der getestet werden soll  #
 * #                                                                         #
 * # Copyright (C) 2008 Michael Plugge <m.plugge@hs-mannheim.de>             #
 * ###########################################################################
 */

DLL_EXPORT int ipi_check(char *zweck)
{
   char c,zweck1[64],check[64],*ptr,*dptr;
   int j;
   UINT4 zahl,rest;

      /* Blanks und Tabs eliminieren */
   for(ptr=zweck,dptr=zweck1;*ptr;ptr++)if(*ptr!=' ' && *ptr!='\t')*dptr++=*ptr;
   *dptr=0;

      /* Verwendungszweck nach check kopieren, dabei Buchstaben konvertieren */
   if(strlen(zweck1)!=20)return IPI_CHECK_INVALID_LENGTH;
   for(ptr=zweck1+2,dptr=check;*ptr;ptr++){
      if((c=*ptr)>='0' && c<='9')
         *dptr++=c;
      else if(c>='A' && c<='Z'){
         c+=10-'A';
         *dptr++=c/10+'0';
         *dptr++=c%10+'0';
      }
      else if(c>='a' && c<='z'){
         c+=10-'a';
         *dptr++=c/10+'0';
         *dptr++=c%10+'0';
      }
   }

      /* Prüfziffer kopieren */
   *dptr++=*zweck;
   *dptr++=*(zweck+1);
   *dptr++=0;

   for(rest=0,ptr=check;*ptr;){
      zahl=rest;
      for(j=0;j<6 && *ptr;ptr++,j++)zahl=zahl*10+ *ptr-'0'; /* maximal sieben weitere Stellen dazu */
      rest=zahl%97;
   }
   if(rest==1)
      return OK;
   else
      return FALSE;
}

/* Hilfsfunktionen für die Suche nach Bankleitzahlen +§§§1 */
/* Überblick +§§§2 */
/* ###########################################################################
 * # Diese Funktionen dienen zum Suchen nach Bankleitzahlen über die anderen #
 * # Felder der LUT-Datei, z.B. Banken in einem bestimmten Ort oder mit      #
 * # einem bestimmten Namen etc.                                             #
 * #                                                                         #
 * # Copyright (C) 2009 Michael Plugge <m.plugge@hs-mannheim.de>             #
 * ###########################################################################
 */

/* Funktion stri_cmp() +§§§2 */
   /* diese Funktion macht dasselbe wie strcasecmp(), ist allerdings portabel:
    * strcmp ohne Groß/Kleinschreibung, dazu noch Umlaute nach ISO-8859-1
    * (diese werden bei den entsprechenden Grundbuchstaben einsortiert).
    */
static inline int stri_cmp(char *a,char *b)
{
   while(*a && lc[UI *a]==lc[UI *b])a++,b++;
   return lc[UI *a]-lc[UI *b];
}

/* Funktion strni_cmp() +§§§2 */
  /* Diese Funktion entspricht weitgehend der vorhergehenden (stri_cmp());
   * falls der String a kürzer ist als b und soweit mit b übereinstimmt, wird
   * allerdings "gleich" (0) zurückgegeben. Diese Funktion wird zum Vergleich
   * bei der Suche nach Orten oder Bankleitzahlen benutzt.
   */
static inline int strni_cmp(char *a,char *b){
   while(*a && lc[UI *a]==lc[UI *b])a++,b++;
   if(*a)
      return lc[UI *a]-lc[UI *b];
   else
      return 0;
}

/* Funktion binary_search_int() +§§§2 */
static int binary_search_int(int a1,int a2,int *base,int *sort_a,int cnt,int *unten,int *anzahl)
{
   int x,y,l,r;

   for(l=0,r=cnt-1,x=(l+r)/2; (a1>(y=base[sort_a[x]]) || a2<y) && l<r;){
      if(a1<y)
         r=x-1;
      else
         l=x+1;
      x=(l+r)/2;
   }
   if(a1>y || a2<y){
      *unten=*anzahl=0;
      return KEY_NOT_FOUND;
   }

   for(l=x;l>=0 && a1<=base[sort_a[l]];l--);
   l++;
   *unten=l;

   for(r=x;r<cnt && a2>=base[sort_a[r]];r++);
   r--;
   *anzahl=r-l+1;
   return OK;
}

/* Funktion binary_search() +§§§2 */
static int binary_search(char *a,char **base,int *sort_a,int cnt,int *unten,int *anzahl)
{
   int x,y,l,r;

   for(l=0,r=cnt-1,x=(l+r)/2;(y=strni_cmp(a,base[sort_a[x]])) && y && l<r;){
      if(y<0)
         r=x-1;
      else
         l=x+1;
      x=(l+r)/2;
   }
   if(y){
      *unten=*anzahl=0;
      return KEY_NOT_FOUND;
   }

   for(l=x;l>=0 && !strni_cmp(a,base[sort_a[l]]);l--);
   l++;
   *unten=l;

   for(r=x;r<cnt && !strni_cmp(a,base[sort_a[r]]);r++);
   r--;
   *anzahl=r-l+1;
   return OK;
}


/* Funktion qcmp_bic() +§§§2 */
static int qcmp_bic(const void *ap,const void *bp)
{
   int a,b,r;

   a=*((int *)ap);
   b=*((int *)bp);
   if((r=stri_cmp(bic[a],bic[b])))
      return r;
   else 
      return a-b;
}

/* Funktion qcmp_name() +§§§2 */
static int qcmp_name(const void *ap,const void *bp)
{
   int a,b,r;

   a=*((int *)ap);
   b=*((int *)bp);
   if((r=stri_cmp(name[a],name[b])))
      return r;
   else 
      return a-b;
}

/* Funktion qcmp_name_kurz() +§§§2 */
static int qcmp_name_kurz(const void *ap,const void *bp)
{
   int a,b,r;

   a=*((int *)ap);
   b=*((int *)bp);
   if((r=stri_cmp(name_kurz[a],name_kurz[b])))
      return r;
   else 
      return a-b;
}

/* Funktion qcmp_ort() +§§§2 */
static int qcmp_ort(const void *ap,const void *bp)
{
   int a,b,r;

   a=*((int *)ap);
   b=*((int *)bp);
   if((r=stri_cmp(ort[a],ort[b])))
      return r;
   else 
      return a-b;
}

/* Funktion qcmp_blz() +§§§2 */
static int qcmp_blz(const void *ap,const void *bp)
{
   int a,b,r;

   a=*((int *)ap);
   b=*((int *)bp);
   if((r=blz[a]-blz[b]))
      return r;
   else 
      return a-b;
}

/* Funktion qcmp_pz_methoden() +§§§2 */
static int qcmp_pz_methoden(const void *ap,const void *bp)
{
   int a,b,r;

   a=*((int *)ap);
   b=*((int *)bp);
   if((r=pz_methoden[a]-pz_methoden[b]))
      return r;
   else 
      return a-b;
}

/* Funktion qcmp_plz() +§§§2 */
static int qcmp_plz(const void *ap,const void *bp)
{
   int a,b,r;

   a=*((int *)ap);
   b=*((int *)bp);
   if((r=plz[a]-plz[b]))
      return r;
   else 
      return a-b;
}

/* Funktion suche_int1() +§§§2 */
static int suche_int1(int a1,int a2,int *anzahl,int **start_idx,int **zweigstelle,int **blz_base,
      int **base_name,int **base_sort,int(*cmp)(const void *, const void *),int cnt)
{
   int i,unten,retval,*b_sort;

   if(!a2)a2=a1;
   b_sort=*base_sort;
   if(!b_sort){
      if(!(b_sort=calloc(cnt+10,sizeof(int*))))return ERROR_MALLOC;
      *base_sort=b_sort;   /* Variable an aufrufende Funktion zurückgeben */
      for(i=0;i<cnt;i++)b_sort[i]=i;
      qsort(b_sort,cnt,sizeof(int*),cmp);
   }
   if((retval=binary_search_int(a1,a2,*base_name,b_sort,cnt,&unten,&cnt))!=OK){
      if(anzahl)*anzahl=0;
      if(start_idx)*start_idx=NULL;
      return retval;
   }
   if(anzahl)*anzahl=cnt;
   if(start_idx)*start_idx=b_sort+unten;
   return OK;
}

/* Funktion suche_int2() +§§§2 */
static int suche_int2(int a1,int a2,int *anzahl,int **start_idx,int **zweigstelle,int **blz_base,
      int **base_name,int **base_sort,int(*cmp)(const void *, const void *))
{
   int i,j,k,n,cnt,unten,retval,*b_sort;

   if(!a2)a2=a1;
   if(startidx[lut2_cnt_hs-1]==lut2_cnt_hs-1){  /* keine Filialen in der Datei enthalten */
      cnt=lut2_cnt_hs;
      blz_f=blz;     /* die einfache BLZ-Tabelle reicht aus */
         /* Zweigstellen-Array, komplett mit Nullen initialisiert (per calloc; keine Zweigstellen) */
      if(!zweigstelle_f && !(zweigstelle_f=calloc(cnt+10,sizeof(int))))return ERROR_MALLOC;
   }
   else
      cnt=lut2_cnt;
   if(!blz_f){   /* Bankleitzahlen mit Filialen; eigenes Array erforderlich */
      if(!(blz_f=calloc(cnt+10,sizeof(int))))return ERROR_MALLOC;
      if(!(zweigstelle_f=calloc(cnt+10,sizeof(int))))return ERROR_MALLOC;
      for(i=j=0;i<lut2_cnt_hs;i++)if((n=filialen[i])>1){
         for(k=0;k<n;k++){
            zweigstelle_f[j]=k;
            blz_f[j++]=blz[i];
         }
      }
      else{
         zweigstelle_f[j]=0;
         blz_f[j++]=blz[i];
      }
   }
   b_sort=*base_sort;
   if(!b_sort){
      if(!(b_sort=calloc(cnt+10,sizeof(int*))))return ERROR_MALLOC;
      *base_sort=b_sort;   /* Variable an aufrufende Funktion zurückgeben */
      for(i=0;i<cnt;i++)b_sort[i]=i;
      qsort(b_sort,cnt,sizeof(int*),cmp);
   }
   if((retval=binary_search_int(a1,a2,*base_name,b_sort,cnt,&unten,&cnt))!=OK){
      if(anzahl)*anzahl=0;
      if(start_idx)*start_idx=NULL;
      return retval;
   }
   if(blz_base)*blz_base=blz_f;
   if(zweigstelle)*zweigstelle=zweigstelle_f;
   if(anzahl)*anzahl=cnt;
   if(start_idx)*start_idx=b_sort+unten;
   return OK;
}

/* Funktion suche_str() +§§§2 */
static int suche_str(char *such_name,int *anzahl,int **start_idx,int **zweigstelle,int **blz_base,
      char ***base_name,int **base_sort,int(*cmp)(const void *, const void *))
{
   int i,j,k,n,cnt,unten,retval,*b_sort;

   if(startidx[lut2_cnt_hs-1]==lut2_cnt_hs-1){  /* keine Filialen in der Datei enthalten */
      cnt=lut2_cnt_hs;
      blz_f=blz;     /* die einfache BLZ-Tabelle reicht aus */
   }
   else
      cnt=lut2_cnt;
   if(!blz_f){   /* Bankleitzahlen mit Filialen; eigenes Array erforderlich */
      if(!(blz_f=calloc(cnt+10,sizeof(int))))return ERROR_MALLOC;
      if(!(zweigstelle_f=calloc(cnt+10,sizeof(int))))return ERROR_MALLOC;
      for(i=j=0;i<lut2_cnt_hs;i++)if((n=filialen[i])>1){
         for(k=0;k<n;k++){
            zweigstelle_f[j]=k;
            blz_f[j++]=blz[i];
         }
      }
      else{
         zweigstelle_f[j]=0;
         blz_f[j++]=blz[i];
      }
   }
   if(blz_base)*blz_base=blz_f;
   if(zweigstelle)*zweigstelle=zweigstelle_f;
   b_sort=*base_sort;
   if(!b_sort){
      if(!(b_sort=calloc(cnt+10,sizeof(int*))))return ERROR_MALLOC;
      *base_sort=b_sort;   /* Variable an aufrufende Funktion zurückgeben */
      for(i=0;i<cnt;i++)b_sort[i]=i;
      qsort(b_sort,cnt,sizeof(int*),cmp);
   }
   while(isspace(*such_name))such_name++;
   if((retval=binary_search(such_name,*base_name,b_sort,cnt,&unten,&cnt))!=OK){
      if(anzahl)*anzahl=0;
      if(start_idx)*start_idx=NULL;
      return retval;
   }
   if(anzahl)*anzahl=cnt;
   if(start_idx)*start_idx=b_sort+unten;
   return OK;
}

/* Funktion lut_suche_bic() +§§§2 */
DLL_EXPORT int lut_suche_bic(char *such_name,int *anzahl,int **start_idx,int **zweigstelle,
      char ***base_name,int **blz_base)
{
   if((init_status&7)<7)return LUT2_NOT_INITIALIZED;
   if(lut_id_status==FALSE)return LUT1_FILE_USED;
   if(!bic)return LUT2_BIC_NOT_INITIALIZED;   
   if(base_name)*base_name=bic;
   return suche_str(such_name,anzahl,start_idx,zweigstelle,blz_base,&bic,&sort_bic,qcmp_bic);
}

/* Funktion lut_suche_namen() +§§§2 */
DLL_EXPORT int lut_suche_namen(char *such_name,int *anzahl,int **start_idx,int **zweigstelle,
      char ***base_name,int **blz_base)
{
   if((init_status&7)<7)return LUT2_NOT_INITIALIZED;
   if(lut_id_status==FALSE)return LUT1_FILE_USED;
   if(!name)return LUT2_NAME_NOT_INITIALIZED;   
   if(base_name)*base_name=name;
   return suche_str(such_name,anzahl,start_idx,zweigstelle,blz_base,&name,&sort_name,qcmp_name);
}

/* Funktion lut_suche_namen_kurz() +§§§2 */
DLL_EXPORT int lut_suche_namen_kurz(char *such_name,int *anzahl,int **start_idx,int **zweigstelle,
      char ***base_name,int **blz_base)
{
   if((init_status&7)<7)return LUT2_NOT_INITIALIZED;
   if(lut_id_status==FALSE)return LUT1_FILE_USED;
   if(!name_kurz)return LUT2_NAME_KURZ_NOT_INITIALIZED;   
   if(base_name)*base_name=name_kurz;
   return suche_str(such_name,anzahl,start_idx,zweigstelle,blz_base,&name_kurz,&sort_name_kurz,qcmp_name_kurz);
}

/* Funktion lut_suche_ort() +§§§2 */
DLL_EXPORT int lut_suche_ort(char *such_name,int *anzahl,int **start_idx,int **zweigstelle,
      char ***base_name,int **blz_base)
{
   if((init_status&7)<7)return LUT2_NOT_INITIALIZED;
   if(lut_id_status==FALSE)return LUT1_FILE_USED;
   if(!ort)return LUT2_ORT_NOT_INITIALIZED;   
   if(base_name)*base_name=ort;
   return suche_str(such_name,anzahl,start_idx,zweigstelle,blz_base,&ort,&sort_ort,qcmp_ort);
}

/* Funktion lut_suche_blz() +§§§2 */
DLL_EXPORT int lut_suche_blz(int such1,int such2,int *anzahl,int **start_idx,int **zweigstelle,int **base_name,int **blz_base)
{
   int cnt;

   if(such2 && such1>such2)return INVALID_SEARCH_RANGE;
   if((init_status&7)<7)return LUT2_NOT_INITIALIZED;
   if(lut_id_status==FALSE)return LUT1_FILE_USED;
   if(base_name)*base_name=blz;
   cnt=lut2_cnt_hs;
   if(blz_base)*blz_base=blz;
   if(zweigstelle){
         /* Dummy-Array für die Zweigstellen anlegen (nur Nullen; für die Rückgabe erforderlich) */
      if(!zweigstelle_f1 && !(zweigstelle_f1=calloc(cnt+10,sizeof(int))))return ERROR_MALLOC;
      *zweigstelle=zweigstelle_f1;
   }
   return suche_int1(such1,such2,anzahl,start_idx,zweigstelle,blz_base,&blz,&sort_blz,qcmp_blz,cnt);
}

/* Funktion lut_suche_pz() +§§§2 */
DLL_EXPORT int lut_suche_pz(int such1,int such2,int *anzahl,int **start_idx,int **zweigstelle,int **base_name,int **blz_base)
{
   int cnt;

   if(such2 && such1>such2)return INVALID_SEARCH_RANGE;
   if((init_status&7)<7)return LUT2_NOT_INITIALIZED;
   if(lut_id_status==FALSE)return LUT1_FILE_USED;
   if(base_name)*base_name=pz_methoden;
   cnt=lut2_cnt_hs;
   if(blz_base)*blz_base=blz;
   if(zweigstelle){
         /* Dummy-Array für die Zweigstellen anlegen (nur Nullen; für die Rückgabe erforderlich) */
      if(!zweigstelle_f1 && !(zweigstelle_f1=calloc(cnt+10,sizeof(int))))return ERROR_MALLOC;
      *zweigstelle=zweigstelle_f1;
   }
   return suche_int1(such1,such2,anzahl,start_idx,zweigstelle,blz_base,&pz_methoden,&sort_pz_methoden,qcmp_pz_methoden,cnt);
}

/* Funktion lut_suche_plz() +§§§2 */
DLL_EXPORT int lut_suche_plz(int such1,int such2,int *anzahl,int **start_idx,int **zweigstelle,int **base_name,int **blz_base)
{
   if(such2 && such1>such2)return INVALID_SEARCH_RANGE;
   if((init_status&7)<7)return LUT2_NOT_INITIALIZED;
   if(lut_id_status==FALSE)return LUT1_FILE_USED;
   if(!plz)return LUT2_PLZ_NOT_INITIALIZED;   
   if(base_name)*base_name=plz;
   return suche_int2(such1,such2,anzahl,start_idx,zweigstelle,blz_base,&plz,&sort_plz,qcmp_plz);
}


#if DEBUG>0
/* Funktion kto_check_test_vars() +§§§1 */
/* ###########################################################################
 * # Die Funktion kto_check_test_vars() macht nichts anderes, als die beiden #
 * # übergebenen Variablen txt und i auszugeben und als String zurückzugeben.#
 * # Sie kann für Debugzwecke benutzt werden, wenn Probleme mit Variablen in #
 * # der DLL auftreten; ansonsten ist sie nicht allzu nützlich. Sie ist      #
 * # allerdings nicht threadfest, da sie mit *einem* statischem Buffer für   #
 * # die Ausgabe arbeitet ;-).                                               #
 * #                                                                         #
 * # Parameter:                                                              #
 * #    txt:        Textvariable                                             #
 * #    i:          Integervariable (4 Byte)                                 #
 * #                                                                         #
 * # Copyright (C) 2006 Michael Plugge <m.plugge@hs-mannheim.de>             #
 * ###########################################################################
 */

DLL_EXPORT char *kto_check_test_vars(char *txt,UINT4 i)
{
   static char test_buffer[200];

   snprintf(test_buffer,200,"Textvariable: %s, Integervariable: %d (Hexwert: 0x%08X)\n",txt,i,i);
   return test_buffer;
}

#endif

#else /* !INCLUDE_KONTO_CHECK_DE */
/* Leerdefinitionen für !INCLUDE_KONTO_CHECK_DE +§§§1 */
#include "konto_check.h"

#define EXCLUDED   {return EXCLUDED_AT_COMPILETIME;} 
#define EXCLUDED_S {return "EXCLUDED_AT_COMPILETIME";}
#define XI DLL_EXPORT int
#define XV DLL_EXPORT void
#define XC DLL_EXPORT char *

XI kto_check_blz(char *blz,char *kto)EXCLUDED
XI kto_check_pz(char *pz,char *kto,char *blz)EXCLUDED
XI kto_check(char *pz_or_blz,char *kto,char *lut_name)EXCLUDED
XI kto_check_t(char *pz_or_blz,char *kto,char *lut_name,KTO_CHK_CTX *ctx)EXCLUDED
XC kto_check_str(char *pz_or_blz,char *kto,char *lut_name)EXCLUDED_S
XC kto_check_str_t(char *pz_or_blz,char *kto,char *lut_name,KTO_CHK_CTX *ctx)EXCLUDED_S
XI cleanup_kto(void)EXCLUDED
XI cleanup_kto_t(KTO_CHK_CTX *ctx)EXCLUDED
XI generate_lut(char *inputname,char *outputname,char *user_info,int lut_version)EXCLUDED
XI get_lut_info(char **info,char *lut_name)EXCLUDED
XI get_lut_info_b(char **info,char *lutname)EXCLUDED;
XI get_lut_info2_b(char *lutname,int *version,char **prolog_p,char **info_p,char **user_info_p)EXCLUDED;
XI get_lut_info_t(char **info,char *lut_name,KTO_CHK_CTX *ctx)EXCLUDED
XI get_lut_info2(char *lut_name,int *version_p,char **prolog_p,char **info_p,char **user_info_p)EXCLUDED
XI get_lut_id(char *lut_name,int set,char *id)EXCLUDED
XC get_kto_check_version(void)EXCLUDED_S
XI create_lutfile(char *name, char *prolog, int slots)EXCLUDED
XI write_lut_block(char *lutname, UINT4 typ,UINT4 len,char *data)EXCLUDED
XI read_lut_block(char *lutname, UINT4 typ,UINT4 *blocklen,char **data)EXCLUDED
XI read_lut_slot(char *lutname, int slot,UINT4 *blocklen,char **data)EXCLUDED
XI lut_dir_dump(char *filename,char *outputname)EXCLUDED
XI generate_lut2_p(char *inputname,char *outputname,char *user_info,char *gueltigkeit,
      UINT4 felder,UINT4 filialen,int slots,int lut_version,int set)EXCLUDED
XI generate_lut2(char *inputname,char *outputname,char *user_info,char *gueltigkeit,
      UINT4 *felder,UINT4 slots,UINT4 lut_version,UINT4 set)EXCLUDED
XI copy_lutfile(char *old_name,char *new_name,int new_slots)EXCLUDED
XI lut_init(char *lut_name,int required,int set)EXCLUDED
XI kto_check_init(char *lut_name,int *required,int **status,int set,int incremental)EXCLUDED
XI kto_check_init_p(char *lut_name,int required,int set,int incremental)EXCLUDED
XI lut_info(char *lut_name,char **info1,char **info2,int *valid1,int *valid2)EXCLUDED
XI lut_valid(void)EXCLUDED
XI lut_multiple(char *b,int *cnt,int **p_blz,char  ***p_name,char ***p_name_kurz,int **p_plz,char ***p_ort,
      int **p_pan,char ***p_bic,int *p_pz,int **p_nr,char **p_aenderung,char **p_loeschung,int **p_nachfolge_blz,
      int *id,int *cnt_all,int **start_idx)EXCLUDED
XI lut_filialen(char *b,int *retval)EXCLUDED
XI dump_lutfile(char *outputname,UINT4 *required)EXCLUDED
XI dump_lutfile_p(char *outputname,UINT4 felder)EXCLUDED
XC lut_name(char *b,int zweigstelle,int *retval)EXCLUDED_S
XC lut_name_i(int b,int zweigstelle,int *retval)EXCLUDED_S
XC lut_name_kurz(char *b,int zweigstelle,int *retval)EXCLUDED_S
XC lut_name_kurz_i(int b,int zweigstelle,int *retval)EXCLUDED_S
XI lut_plz(char *b,int zweigstelle,int *retval)EXCLUDED
XI lut_plz_i(int b,int zweigstelle,int *retval)EXCLUDED
XC lut_ort(char *b,int zweigstelle,int *retval)EXCLUDED_S
XC lut_ort_i(int b,int zweigstelle,int *retval)EXCLUDED_S
XI lut_pan(char *b,int zweigstelle,int *retval)EXCLUDED
XI lut_pan_i(int b,int zweigstelle,int *retval)EXCLUDED
XC lut_bic(char *b,int zweigstelle,int *retval)EXCLUDED_S
XC lut_bic_i(int b,int zweigstelle,int *retval)EXCLUDED_S
XI lut_nr(char *b,int zweigstelle,int *retval)EXCLUDED
XI lut_nr_i(int b,int zweigstelle,int *retval)EXCLUDED
XI lut_pz(char *b,int zweigstelle,int *retval)EXCLUDED
XI lut_pz_i(int b,int zweigstelle,int *retval)EXCLUDED
XI lut_aenderung(char *b,int zweigstelle,int *retval)EXCLUDED
XI lut_aenderung_i(int b,int zweigstelle,int *retval)EXCLUDED
XI lut_loeschung(char *b,int zweigstelle,int *retval)EXCLUDED
XI lut_loeschung_i(int b,int zweigstelle,int *retval)EXCLUDED
XI lut_nachfolge_blz(char *b,int zweigstelle,int *retval)EXCLUDED
XI lut_nachfolge_blz_i(int b,int zweigstelle,int *retval)EXCLUDED
XI lut_cleanup(void)EXCLUDED
XC kto_check_retval2txt(int retval)EXCLUDED_S
XC kto_check_retval2txt_short(int retval)EXCLUDED_S
XC kto_check_retval2html(int retval)EXCLUDED_S
XC kto_check_retval2dos(int retval)EXCLUDED_S
XC kto_check_retval2utf8(int retval)EXCLUDED_S
XI rebuild_blzfile(char *inputname,char *outputname,UINT4 set)EXCLUDED
XI iban_check(char *iban,int *retval)EXCLUDED
XC iban2bic(char *iban,int *retval,char *blz,char *kto)EXCLUDED_S
XC iban_gen(char *kto,char *blz,int *retval)EXCLUDED_S
XI ipi_gen(char *zweck,char *dst,char *papier)EXCLUDED
XI ipi_check(char *zweck)EXCLUDED
XI kto_check_blz_dbg(char *blz,char *kto,RETVAL *retvals)EXCLUDED
XI kto_check_pz_dbg(char *pz,char *kto,char *blz,RETVAL *retvals)EXCLUDED
XC kto_check_test_vars(char *txt,UINT4 i)EXCLUDED_S
XI set_verbose_debug(int mode)EXCLUDED
XI lut_suche_bic(char *such_name,int *anzahl,int **start_idx,char ***base_name,int **blz_base)EXCLUDED
XI lut_suche_namen(char *such_name,int *anzahl,int **start_idx,char ***base_name,int **blz_base)EXCLUDED
XI lut_suche_namen_kurz(char *such_name,int *anzahl,int **start_idx,char ***base_name,int **blz_base)EXCLUDED
XI lut_suche_ort(char *such_name,int *anzahl,int **start_idx,char ***base_name,int **blz_base)EXCLUDED
XI lut_suche_blz(int a1,int a2,int *anzahl,int **start_idx,int **base_name,int **blz_base)EXCLUDED
XI lut_suche_pz(int a1,int a2,int *anzahl,int **start_idx,int **base_name,int **blz_base)EXCLUDED
XI lut_suche_plz(int a1,int a2,int *anzahl,int **start_idx,int **base_name,int **blz_base)EXCLUDED
#endif

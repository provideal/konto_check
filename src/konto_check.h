#line 6 "konto_check_h.lx"
/*
 * ##########################################################################
 * #  Dies ist konto_check, ein Programm zum Testen der Prüfziffern         #
 * #  von deutschen Bankkonten. Es kann als eigenständiges Programm         #
 * #  (z.B. mit der beigelegten main() Routine) oder als Library zur        #
 * #  Verwendung in anderen Programmen bzw. Programmiersprachen benutzt     #
 * #  werden.                                                               #
 * #                                                                        #
 * #  Copyright (C) 2002-2007 Michael Plugge <m.plugge@hs-mannheim.de>      #
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
 * #                                                                        #
 * ##########################################################################
 */

#ifndef KONTO_CHECK_H_INCLUDED
#define KONTO_CHECK_H_INCLUDED

/* 
 * ##########################################################################
 * # Fallls das folgende Makro auf 1 gesetzt wird, werden unterschiedliche  #
 * # Interpretationen der Prüfziffermethoden interpretiert wie in BAV       #
 * # (Bank Account Validator, http://sourceforge.net/projects/bav)          #
 * # Dieses Makro dient zum Test der beiden Pakete, damit bei den Tests     # 
 * # nicht immer Unterschiede ausgegeben werden, wo nur (bekannte)          #
 * # unterschiedliche Interpretationen der Berechnungsmethoden existieren.  #
 * ##########################################################################
 */
#ifndef BAV_KOMPATIBEL
#define BAV_KOMPATIBEL 0
#endif

/*
 * ##########################################################################
 * # Fallls das Makro DEBUG auf 1 gesetzt wird, werden zwei- und drei-      #
 * # stellige Methoden (Methode + evl. Untermethode) akzeptiert.            #
 * # Außerdem wird die Prüfziffer (pz) als globale Variable deklariert.     #
 * ##########################################################################
 */
#ifndef DEBUG
#define DEBUG 1
#endif

/*
 * ##########################################################################
 * # falls das folgende Makro auf 1 gesetzt wird, werden für das PHP-Modul  #
 * # symbolische Konstanten definiert (analog zu den #define's aus der      #
 * # C Bibliothek. Der Wert false ist in PHP allerdings schon belegt und    #
 * # kann nicht verwendet werden; stattdessen wird NOT_OK definiert.        #
 * # gesamte Datei nach BLZs sortiert wird).                                #
 * ##########################################################################
 */
#define SYMBOLIC_RETVALS 1

/*
 * ##########################################################################
 * # falls das folgende Makro auf 1 gesetzt wird, werden Dummys für die     #
 * # alten globalen Variablen eingebunden; die alte Funktionalität wird     #
 * # jedoch aufgrund der Threadfestigkeit nicht implementiert.              #
 * ##########################################################################
 */
#define INCLUDE_DUMMY_GLOBALS 0

/*
 * ##########################################################################
 * # falls das folgende Makro auf 1 gesetzt wird, werden die Zweigstellen   #
 * # in der LUT-Datei nach Postleitzahlen sortiert; andernfalls wird die    #
 * # Reihenfolge aus der Datei der Deutschen Bundesbank übernommen (mit der #
 * # Ausnahme, daß Hauptstellen vor die Zweigstellen gesetzt werden und die #
 * # gesamte Datei nach BLZs sortiert wird).                                #
 * ##########################################################################
 */
#define SORT_PLZ 0

/*
 * ######################################################################
 * # DLL-Optionen für Windows                                           #
 * # Der DLL-Code wurde aus der Datei dllhelpers (beim MinGW-Compiler   #
 * # enthalten, http://www.mingw.org/) entnommen                        #
 * #                                                                    #
 * # Falls das Makro USE_CDECL gesetzt ist, wird als Aufrufmethode      #
 * # CDECL genommen, ansonsten STDCALL (Default).                       #
 * ######################################################################
 */

#ifdef _WIN32
#  if USE_CDECL
#    if BUILD_DLL /* DLL kompilieren */
#      define DLL_EXPORT __declspec (dllexport)
#      define DLL_EXPORT_V __declspec (dllexport)
#    elif USE_DLL /* DLL in einem anderen Programm benutzen */
#      define DLL_EXPORT __declspec (dllimport)
#      define DLL_EXPORT_V __declspec (dllimport)
#    else /* kein DLL-Krempel erforderlich */
#      define DLL_EXPORT
#      define DLL_EXPORT_V
#    endif
#  else
#     if BUILD_DLL /* DLL kompilieren */
#      define DLL_EXPORT __declspec (dllexport) __stdcall 
#      define DLL_EXPORT_V __declspec (dllexport)
#    elif USE_DLL /* DLL in einem anderen Programm benutzen */
#      define DLL_EXPORT __declspec (dllimport) __stdcall 
#      define DLL_EXPORT_V __declspec (dllimport)
#    else /* kein DLL-Krempel erforderlich */
#      define DLL_EXPORT
#      define DLL_EXPORT_V
#    endif
#  endif
#  define localtime_r(timep,result) localtime(timep)
#else
#  define DLL_EXPORT
#  define DLL_EXPORT_V
#endif

/*
 * ######################################################################
 * #          Defaultnamen und Suchpfad für die LUT-Datei               #
 * ######################################################################
 */

#define DEFAULT_LUT_NAME "blz.lut","blz.lut2","blz.lut1"

#if _WIN32>0
#define DEFAULT_LUT_PATH ".","C:","C:\\Programme\\konto_check"
#else
#define DEFAULT_LUT_PATH ".","/usr/local/etc","/etc","/usr/local/bin","/opt/konto_check"
#endif

   /* maximale Länge für Default-Suchpfad und Dateiname der LUT-Datei */
#define LUT_PATH_LEN 128

/*
 * ######################################################################
 * #               Felder für die LUT-Datei (ab LUT-Version 2.0)        #
 * ######################################################################
 */

#define DEFAULT_LUT_FIELDS_NUM   5
#define DEFAULT_LUT_FIELDS       lut_set_5
#define DEFAULT_LUT_VERSION      3
#define DEFAULT_SLOTS            40
#define DEFAULT_INIT_LEVEL       5
#define LAST_LUT_BLOCK           100

   /* Das folgende Makro bestimmt das Verhalten, wenn zu einer LUT-Datei Blocks
    * hinzugefügt werden sollen und bereits (mindestens) ein Block mit
    * demselben Typ in der Datei enthalten ist. Falls das Makro 1 ist, wird für
    * den neuen Block der alte Slots der LUT-Datei benutzt; bei 0 wird ein
    * neuer Slot allokiert.
    *
    * Falls das Makro 0 ist, kann man auch später noch auf alte Blocks
    * zugreifen (falls das einmal notwendig sein sollte); allerdings läßt sich
    * das LUT-Verzeichnis nicht vergrößern, so daß u.U. nach mehreren Updates
    * alle Slots belegt sind und daher keine neuen Blocks mehr geschrieben
    * werden können.
    */

#define REPLACE_LUT_DIR_ENTRIES 1

#define LUT2_BLZ                      1
#define LUT2_FILIALEN                 2
#define LUT2_NAME                     3
#define LUT2_PLZ                      4
#define LUT2_ORT                      5
#define LUT2_NAME_KURZ                6
#define LUT2_PAN                      7
#define LUT2_BIC                      8
#define LUT2_PZ                       9
#define LUT2_NR                      10
#define LUT2_AENDERUNG               11
#define LUT2_LOESCHUNG               12
#define LUT2_NACHFOLGE_BLZ           13
#define LUT2_NAME_NAME_KURZ          14
#define LUT2_INFO                    15

#define LUT2_2_BLZ                  101
#define LUT2_2_FILIALEN             102
#define LUT2_2_NAME                 103
#define LUT2_2_PLZ                  104
#define LUT2_2_ORT                  105
#define LUT2_2_NAME_KURZ            106
#define LUT2_2_PAN                  107
#define LUT2_2_BIC                  108
#define LUT2_2_PZ                   109
#define LUT2_2_NR                   110
#define LUT2_2_AENDERUNG            111
#define LUT2_2_LOESCHUNG            112
#define LUT2_2_NACHFOLGE_BLZ        113
#define LUT2_2_NAME_NAME_KURZ       114
#define LUT2_2_INFO                 115

#ifdef KONTO_CHECK_VARS
char *lut2_feld_namen[256];
#else
extern char *lut2_feld_namen[256];
#endif

/*
 * ######################################################################
 * #               mögliche Rückgabewerte von kto_check() & Co          #
 * ######################################################################
 */

#undef FALSE
#define INVALID_SEARCH_RANGE                   -79
#define KEY_NOT_FOUND                          -78
#define BAV_FALSE                              -77
#define LUT2_NO_USER_BLOCK                     -76
#define INVALID_SET                            -75
#define NO_GERMAN_BIC                          -74
#define IPI_CHECK_INVALID_LENGTH               -73
#define IPI_INVALID_CHARACTER                  -72
#define IPI_INVALID_LENGTH                     -71
#define LUT1_FILE_USED                         -70
#define MISSING_PARAMETER                      -69
#define IBAN2BIC_ONLY_GERMAN                   -68
#define IBAN_OK_KTO_NOT                        -67
#define KTO_OK_IBAN_NOT                        -66
#define TOO_MANY_SLOTS                         -65
#define INIT_FATAL_ERROR                       -64
#define INCREMENTAL_INIT_NEEDS_INFO            -63
#define INCREMENTAL_INIT_FROM_DIFFERENT_FILE   -62
#define DEBUG_ONLY_FUNCTION                    -61
#define LUT2_INVALID                           -60
#define LUT2_NOT_YET_VALID                     -59
#define LUT2_NO_LONGER_VALID                   -58
#define LUT2_GUELTIGKEIT_SWAPPED               -57
#define LUT2_INVALID_GUELTIGKEIT               -56
#define LUT2_INDEX_OUT_OF_RANGE                -55
#define LUT2_INIT_IN_PROGRESS                  -54
#define LUT2_BLZ_NOT_INITIALIZED               -53
#define LUT2_FILIALEN_NOT_INITIALIZED          -52
#define LUT2_NAME_NOT_INITIALIZED              -51
#define LUT2_PLZ_NOT_INITIALIZED               -50
#define LUT2_ORT_NOT_INITIALIZED               -49
#define LUT2_NAME_KURZ_NOT_INITIALIZED         -48
#define LUT2_PAN_NOT_INITIALIZED               -47
#define LUT2_BIC_NOT_INITIALIZED               -46
#define LUT2_PZ_NOT_INITIALIZED                -45
#define LUT2_NR_NOT_INITIALIZED                -44
#define LUT2_AENDERUNG_NOT_INITIALIZED         -43
#define LUT2_LOESCHUNG_NOT_INITIALIZED         -42
#define LUT2_NACHFOLGE_BLZ_NOT_INITIALIZED     -41
#define LUT2_NOT_INITIALIZED                   -40
#define LUT2_FILIALEN_MISSING                  -39
#define LUT2_PARTIAL_OK                        -38
#define LUT2_Z_BUF_ERROR                       -37
#define LUT2_Z_MEM_ERROR                       -36
#define LUT2_Z_DATA_ERROR                      -35
#define LUT2_BLOCK_NOT_IN_FILE                 -34
#define LUT2_DECOMPRESS_ERROR                  -33
#define LUT2_COMPRESS_ERROR                    -32
#define LUT2_FILE_CORRUPTED                    -31
#define LUT2_NO_SLOT_FREE                      -30
#define UNDEFINED_SUBMETHOD                    -29
#define EXCLUDED_AT_COMPILETIME                -28
#define INVALID_LUT_VERSION                    -27
#define INVALID_PARAMETER_STELLE1              -26
#define INVALID_PARAMETER_COUNT                -25
#define INVALID_PARAMETER_PRUEFZIFFER          -24
#define INVALID_PARAMETER_WICHTUNG             -23
#define INVALID_PARAMETER_METHODE              -22
#define LIBRARY_INIT_ERROR                     -21
#define LUT_CRC_ERROR                          -20
#define FALSE_GELOESCHT                        -19
#define OK_NO_CHK_GELOESCHT                    -18
#define OK_GELOESCHT                           -17
#define BLZ_GELOESCHT                          -16
#define INVALID_BLZ_FILE                       -15
#define LIBRARY_IS_NOT_THREAD_SAFE             -14
#define FATAL_ERROR                            -13
#define INVALID_KTO_LENGTH                     -12
#define FILE_WRITE_ERROR                       -11
#define FILE_READ_ERROR                        -10
#define ERROR_MALLOC                            -9
#define NO_BLZ_FILE                             -8
#define INVALID_LUT_FILE                        -7
#define NO_LUT_FILE                             -6
#define INVALID_BLZ_LENGTH                      -5
#define INVALID_BLZ                             -4
#define INVALID_KTO                             -3
#define NOT_IMPLEMENTED                         -2
#define NOT_DEFINED                             -1
#define FALSE                                    0
#define OK                                       1
#define OK_NO_CHK                                2
#define OK_TEST_BLZ_USED                         3
#define LUT2_VALID                               4
#define LUT2_NO_VALID_DATE                       5
#define LUT1_SET_LOADED                          6
#define LUT1_FILE_GENERATED                      7
#line 204 "konto_check_h.lx"

#define MAX_BLZ_CNT 30000  /* maximale Anzahl BLZ's in generate_lut() */

#ifndef INT4_DEFINED
#define INT4_DEFINED
#include <limits.h>
#if INT_MAX==2147483647
typedef int INT4;
typedef unsigned int UINT4;
#elif LONG_MAX==2147483647
typedef long INT4;
typedef unsigned long UINT4;
#else  /* Notausstieg, kann 4 Byte Integer nicht bestimmen */
#error "Typedef für 4 Byte Integer nicht definiert"
#endif
#endif

   /* in den alten Versionen war reserved als 'void *reserved[5]' definiert;
    * es ist allerdings geschickter, einen Teil davon als char-Array zu
    * definieren.  Dieses kann dann flexibler verwendet werden (auch
    * byteweise). Die Größe der Struktur wird auf diese Weise nicht verändert.
    *
    * Als erstes neues Element wird pz_pos (Position der Prüfziffer) eingeführt.
    */
typedef struct{
   char *methode;
   INT4 pz_methode;
   INT4 pz;
   signed char pz_pos;
   char reserved_chr[3*sizeof(void*)-1];
   void *reserved_ptr[2];
} RETVAL;


/* ######################################################################
 * # Dies ist der alte Kommentar zu KTO_CHK_CTX; die Struktur ist ab    #
 * # Version 3.0 obsolet und wird nicht mehr verwendet. Die Deklaration #
 * # ist allerdings noch in der Headerdatei enthalten, um Abwärtskompa- #
 * # tibilität mit dem alten Interface zu wahren; Funktionen, die die   #
 * # Struktur benutzen, rufen einfach die neuen (threadfesten)          #
 * # Funktionen auf; die ctx-Variable wird dabei einfach ignoriert.     #
 * #                                                                    #
 * # Definition der Struktur KTO_CHK_CTX. Diese Struktur enthält alle   #
 * # globalen bzw. static Variablen der alten Library und wird bei den  #
 * # threadfesten Varianten als Parameter übergeben. Damit treten keine #
 * # Interferenzen zwischen verschiedenen Instanzen bei einem gleich-   #
 * # zeitigen Aufruf der library mehr auf, wie es bei den nicht thread- #
 * # festen Varianten der Fall ist (beispielsweise werden kto_check_msg,#
 * # pz_str, pz_methode und pz von jeder Instanz überschrieben; dadurch #
 * # sind diese Variablen in einem Thread-Kontext unbrauchbar.          #
 * # Die alten (nicht threadfesten) Varianten sind so realisiert, daß   #
 * # eine (static) globale Struktur global_ctx definiert wird, die von  #
 * # den diesen Funktionen benutzt wird. Diese Vorgehensweise ist       #
 * # wesentlich schneller als die Alternative, lokale Variablen für die #
 * # Problemfälle zu benutzen; die Umsetzung zwischen nicht threadfesten#
 * # und threadfesten Variablen geschieht über Präprozessor #defines    #
 * # in konto_check.c.                                                  #
 * ######################################################################
 */
typedef struct{
   char *kto_check_msg,pz_str[4];
   int pz_methode;
   int pz;
   UINT4 cnt_blz,*blz_array,*pz_array,*blz_hash_low,*blz_hash_high,*invalid;
   char lut_info[1024];
   UINT4 b1[256],b2[256],b3[256],b4[256],b5[256],b6[256],b7[256],b8[256];
   int c2,d2,a5,p,konto[11];
} KTO_CHK_CTX;

/*
 * ######################################################################
 * # kto_check(): Test eines Kontos                                     #
 * #              Diese Funktion stammt aus der alten Programmier-      #
 * #              schnittstelle und ist aus Kompatibilitätsgründen noch #
 * #              in der Library enthalten. Da alle möglichen Fälle     #
 * #              behandelt werden und Initialisierung und Test nicht   #
 * #              getrennt sind, hat diese Funktion im Vergleich zu dem #
 * #              neuen Interface einen relativ hohen Overhead, und     #
 * #              sollte durch die neuen Funktionen (s.u.) ersetzt      #
 * #              werden.                                               #
 * #                                                                    #
 * # Parameter: x_blz:      falls 2-stellig: Prüfziffer                 #
 * #                        falls 8-stellig: Bankleitzahl               #
 * #                                                                    #
 * #            kto:        Kontonummer (wird vor der Berechnung        #
 * #                        linksbündig mit Nullen auf 10 Stellen       #
 * #                        aufgefüllt)                                 #
 * #                                                                    #
 * #            lut_name:   Dateiname der Lookup-Tabelle.               #
 * #                        Falls NULL oder ein leerer String übergeben #
 * #                        wird, wird DEFAULT_LUT_NAME benutzt.        #
 * #                                                                    #
 * # Rückgabewerte: s.o.                                                #
 * ######################################################################
 */
DLL_EXPORT int kto_check(char *x_blz,char *kto,char *lut_name);
DLL_EXPORT int kto_check_t(char *x_blz,char *kto,char *lut_name,KTO_CHK_CTX *ctx);
DLL_EXPORT char *kto_check_str(char *x_blz,char *kto,char *lut_name);
DLL_EXPORT char *kto_check_str_t(char *x_blz,char *kto,char *lut_name,KTO_CHK_CTX *ctx);

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

DLL_EXPORT int kto_check_blz(char *blz,char *kto);
#if DEBUG>0
DLL_EXPORT int kto_check_blz_dbg(char *blz,char *kto,RETVAL *retvals);
#endif

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
 * #    blz:        Bankleitzahl (immer 8-stellig)                           #
 * #    kto:        Kontonummer                                              #
 * #                                                                         #
 * # Copyright (C) 2007 Michael Plugge <m.plugge@hs-mannheim.de>             #
 * ###########################################################################
 */

DLL_EXPORT int kto_check_pz(char *pz,char *kto,char *blz);
#if DEBUG>0
DLL_EXPORT int kto_check_pz_dbg(char *pz,char *kto,char *blz,RETVAL *retvals);
#endif

/*
 * ######################################################################
 * # cleanup_kto(): Aufräumarbeiten                                     #
 * #                                                                    #
 * # Die Funktion gibt allokierten Speicher frei und setzt die Variable #
 * # cnt_blz auf 0, um anzuzeigen, daß die Library bei Bedarf neu       #
 * # initialisiert werden muß.                                          #
 * #                                                                    #
 * # Rückgabewerte: 0: es war nichts zu tun (library wurde nicht init.) #
 * #                1: Aufräumen fertig                                 #
 * ######################################################################
 */
DLL_EXPORT int cleanup_kto(void);
DLL_EXPORT int cleanup_kto_t(KTO_CHK_CTX *ctx);

/*
 * ######################################################################
 * # generate_lut(): Lookup-Table generieren                            #
 * #                                                                    #
 * # Die Funktion generiert die Datei blz.lut, die alle Bankleitzahlen  #
 * # und die zugehörigen Prüfziffermethoden in komprimierter Form       #
 * # enthält.                                                           #
 * #                                                                    #
 * # Parameter: inputname:  Name der Bankleitzahlendatei der Deutschen  #
 * #                        Bundesbank (z.B. blz0303pc.txt)             #
 * #                                                                    #
 * #            outputname: Name der Zieldatei (z.B. blz.lut)           #
 * #                                                                    #
 * #            user_info:  Info-Zeile, die zusätzlich in die LUT-Datei #
 * #                        geschrieben wird. Diese Zeile wird von der  #
 * #                        Funktion get_lut_info() in zurückgegeben,   #
 * #                        aber ansonsten nicht ausgewertet.           #
 * #                                                                    #
 * #                                                                    #
 * #                                                                    #
 * #           lut_version: Format der LUT-Datei. Mögliche Werte:       #
 * #                        1: altes Format (1.0)                       #
 * #                        2: altes Format (1.1) mit Infozeile         #
 * #                        3: neues Format (2.0) mit Blocks            #
 * #                                                                    #
 * # Rückgabewerte:                                                     #
 * #    NO_BLZ_FILE          Bankleitzahlendatei nicht gefunden         #
 * #    FILE_WRITE_ERROR     kann Datei nicht schreiben (Schreibschutz?)#
 * #    OK                   Erfolg                                     #
 * ######################################################################
 */
DLL_EXPORT int generate_lut(char *inputname,char *outputname,char *user_info,int lut_version);

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
 * # Rückgabewerte: wie in read_lut():                                  #
 * #    ERROR_MALLOC       kann keinen Speicher allokieren              #
 * #    NO_LUT_FILE        LUT-Datei nicht gefunden (Pfad falsch?)      #
 * #    FATAL_ERROR        kann die LUT-Datei nicht lesen               #
 * #    INVALID_LUT_FILE   Fehler in der LUT-Datei (Format, CRC...)     #
 * #    OK                 Erfolg                                       #
 * ######################################################################
 */
DLL_EXPORT int get_lut_info(char **info,char *lut_name);
DLL_EXPORT int get_lut_info_t(char **info,char *lut_name,KTO_CHK_CTX *ctx);

/*
 * ######################################################################
 * # get_kto_check_version(): Version und Releasedate der library holen #
 * # Diese Funktion wird erst ab Version 1.1 der library unterstützt.   #
 * ######################################################################
 */
DLL_EXPORT char *get_kto_check_version(void);

#if DEBUG>0
/* ###########################################################################
 * # Die Funktion kto_check_test_vars() macht nichts anderes, als die beiden #
 * # übergebenen Variablen txt und i auszugeben und als String zurückzugeben.#
 * # Sie kann für Debugzwecke benutzt werden, wenn Probleme mit Variablen in #
 * # der DLL auftreten; ansonsten ist sie nicht allzu nützlich.              #
 * #                                                                         #
 * # Parameter:                                                              #
 * #    txt:        Textvariable                                             #
 * #    i:          Integervariable (4 Byte)                                 #
 * #    ip:         Pointer auf Integerarray (4 Byte Integer-Werte)          #
 * ###########################################################################
 */

DLL_EXPORT char *kto_check_test_vars(char *txt,UINT4 i);
DLL_EXPORT int set_verbose_debug(int mode);
#endif

/*
 * ######################################################################
 * # public interface der lut2-Routinen                                 #
 * # Eine nähere Beschreibung findet sich momentan nur im C-Code, sie   #
 * # wird aber später nachgeliefert.                                    #
 * ######################################################################
 */
   /* public interface von lut2 */
DLL_EXPORT int create_lutfile(char *name, char *prolog, int slots);
DLL_EXPORT int write_lut_block(char *lutname,UINT4 typ,UINT4 len,char *data);
DLL_EXPORT int read_lut_block(char *lutname, UINT4 typ,UINT4 *blocklen,char **data);
DLL_EXPORT int read_lut_slot(char *lutname,int slot,UINT4 *blocklen,char **data);
DLL_EXPORT int lut_dir_dump(char *lutname,char *outputname);
DLL_EXPORT int generate_lut2_p(char *inputname,char *outputname,char *user_info,char *gueltigkeit,
      UINT4 felder,UINT4 filialen,int slots,int lut_version,int set);
DLL_EXPORT int generate_lut2(char *inputname,char *outputname,char *user_info,
      char *gueltigkeit,UINT4 *felder,UINT4 slots,UINT4 lut_version,UINT4 set);
DLL_EXPORT int copy_lutfile(char *old_name,char *new_name,int new_slots);
DLL_EXPORT int lut_init(char *lut_name,int required,int set);
DLL_EXPORT int kto_check_init(char *lut_name,int *required,int **status,int set,int incremental);
DLL_EXPORT int kto_check_init2(char *lut_name);
DLL_EXPORT int *lut2_status(void);
DLL_EXPORT int kto_check_init_p(char *lut_name,int required,int set,int incremental);
DLL_EXPORT int lut_info(char *lut_name,char **info1,char **info2,int *valid1,int *valid2);
DLL_EXPORT int lut_valid(void);
DLL_EXPORT int get_lut_info2(char *lut_name,int *version_p,char **prolog_p,char **info_p,char **user_info_p);
DLL_EXPORT int get_lut_info_b(char **info,char *lutname);
DLL_EXPORT int get_lut_info2_b(char *lutname,int *version,char **prolog_p,char **info_p,char **user_info_p);
DLL_EXPORT int get_lut_id(char *lut_name,int set,char *id);
DLL_EXPORT int rebuild_blzfile(char *inputname,char *outputname,UINT4 set);
DLL_EXPORT int dump_lutfile(char *outputname,UINT4 *required);
DLL_EXPORT int dump_lutfile_p(char *outputname,UINT4 felder);

   /* Universalfunktion, die Pointer auf die internen Variablen zurückliefert (von Haupt- und Nebenstellen) */
DLL_EXPORT int lut_multiple(char *b,int *cnt,int **p_blz,char  ***p_name,char ***p_name_kurz,int **p_plz,char ***p_ort,
      int **p_pan,char ***p_bic,int *p_pz,int **p_nr,char **p_aenderung,char **p_loeschung,int **p_nachfolge_blz,
      int *id,int *cnt_all,int **start_idx);
DLL_EXPORT int lut_multiple_i(int b,int *cnt,int **p_blz,char  ***p_name,char ***p_name_kurz,
      int **p_plz,char ***p_ort,int **p_pan,char ***p_bic,int *p_pz,int **p_nr,
      char **p_aenderung,char **p_loeschung,int **p_nachfolge_blz,int *id,
      int *cnt_all,int **start_idx);

   /* Funktionen, um einzelne Felder zu bestimmen (Rückgabe direkt) */
DLL_EXPORT int lut_filialen(char *b,int *retval);
DLL_EXPORT int lut_filialen_i(int b,int *retval);
DLL_EXPORT char *lut_name(char *b,int zweigstelle,int *retval);
DLL_EXPORT char *lut_name_i(int b,int zweigstelle,int *retval);
DLL_EXPORT char *lut_name_kurz(char *b,int zweigstelle,int *retval);
DLL_EXPORT char *lut_name_kurz_i(int b,int zweigstelle,int *retval);
DLL_EXPORT int lut_plz(char *b,int zweigstelle,int *retval);
DLL_EXPORT int lut_plz_i(int b,int zweigstelle,int *retval);
DLL_EXPORT char *lut_ort(char *b,int zweigstelle,int *retval);
DLL_EXPORT char *lut_ort_i(int b,int zweigstelle,int *retval);
DLL_EXPORT int lut_pan(char *b,int zweigstelle,int *retval);
DLL_EXPORT int lut_pan_i(int b,int zweigstelle,int *retval);
DLL_EXPORT char *lut_bic(char *b,int zweigstelle,int *retval);
DLL_EXPORT char *lut_bic_i(int b,int zweigstelle,int *retval);
DLL_EXPORT int lut_nr(char *b,int zweigstelle,int *retval);
DLL_EXPORT int lut_nr_i(int b,int zweigstelle,int *retval);
DLL_EXPORT int lut_pz(char *b,int zweigstelle,int *retval);
DLL_EXPORT int lut_pz_i(int b,int zweigstelle,int *retval);
DLL_EXPORT int lut_aenderung(char *b,int zweigstelle,int *retval);
DLL_EXPORT int lut_aenderung_i(int b,int zweigstelle,int *retval);
DLL_EXPORT int lut_loeschung(char *b,int zweigstelle,int *retval);
DLL_EXPORT int lut_loeschung_i(int b,int zweigstelle,int *retval);
DLL_EXPORT int lut_nachfolge_blz(char *b,int zweigstelle,int *retval);
DLL_EXPORT int lut_nachfolge_blz_i(int b,int zweigstelle,int *retval);

   /* Suche von BLZs */
DLL_EXPORT int lut_suche_bic(char *such_name,int *anzahl,int **start_idx,int **zweigstelle,char ***base_name,int **blz_base);
DLL_EXPORT int lut_suche_namen(char *such_name,int *anzahl,int **start_idx,int **zweigstelle,char ***base_name,int **blz_base);
DLL_EXPORT int lut_suche_namen_kurz(char *such_name,int *anzahl,int **start_idx,int **zweigstelle,char ***base_name,int **blz_base);
DLL_EXPORT int lut_suche_ort(char *such_name,int *anzahl,int **start_idx,int **zweigstelle,char ***base_name,int **blz_base);
DLL_EXPORT int lut_suche_blz(int such1,int such2,int *anzahl,int **start_idx,int **zweigstelle,int **base_name,int **blz_base);
DLL_EXPORT int lut_suche_pz(int such1,int such2,int *anzahl,int **start_idx,int **zweigstelle,int **base_name,int **blz_base);
DLL_EXPORT int lut_suche_plz(int such1,int such2,int *anzahl,int **start_idx,int **zweigstelle,int **base_name,int **blz_base);

   /* Aufräumarbeiten */
DLL_EXPORT int lut_cleanup(void);

   /* IBAN-Sachen */
DLL_EXPORT int iban_check(char *iban,int *retval);
DLL_EXPORT char *iban2bic(char *iban,int *retval,char *blz,char *kto);
DLL_EXPORT char *iban_gen(char *kto,char *blz,int *retval);
DLL_EXPORT int ipi_gen(char *zweck,char *dst,char *papier);
DLL_EXPORT int ipi_check(char *zweck);

   /* Rückgabewerte in Klartext umwandeln */
DLL_EXPORT char *kto_check_retval2txt(int retval);
DLL_EXPORT char *kto_check_retval2txt_short(int retval);
DLL_EXPORT char *kto_check_retval2html(int retval);
DLL_EXPORT char *kto_check_retval2utf8(int retval);
DLL_EXPORT char *kto_check_retval2dos(int retval);

/*
 * ######################################################################
 * #               globale Variablen                                    #
 * ######################################################################
 */

#ifndef KONTO_CHECK_VARS
#if DEBUG>0
   /* "aktuelles" Datum für die Testumgebung (um einen Datumswechsel zu simulieren) */
DLL_EXPORT_V extern UINT4 current_date;
#endif

/*
 * ######################################################################
 * # die folgenden globalen Variablen waren in Version 1 und 2 von      #
 * # konto_check definiert; ab Version 3 werden sie nicht mehr unter-   #
 * # stützt. Zur Vermeidung von Linker-Fehlermeldungen können jedoch    #
 * # Dummyversionen eingebunden werden (ohne Funktionalität).           #
 * ######################################################################
 */

#if INCLUDE_DUMMY_GLOBALS>0
DLL_EXPORT_V extern const char *kto_check_msg;   /* globaler char-ptr mit Klartext-Ergebnis des Tests */
DLL_EXPORT_V extern const char pz_str[];         /* benutzte Prüfziffer-Methode und -Untermethode (als String) */
DLL_EXPORT_V extern int pz_methode; /* pz_methode: benutzte Prüfziffer-Methode (numerisch) */
#if DEBUG>0
DLL_EXPORT_V extern int pz;                /* Prüfziffer (bei DEBUG als globale Variable für Testzwecke) */
#endif   /* DEBUG */
#endif   /* INCLUDE_DUMMY_GLOBALS */
#endif   /* KONTO_CHECK_VARS */
#endif   /* KONTO_CHECK_H_INCLUDED */

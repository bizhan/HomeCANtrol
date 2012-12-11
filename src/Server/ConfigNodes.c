/* ConfigNodes.c: enthaelt alle Routinen zum Konfigurieren der Netzwerkknoten
                  anhand der Config.xml-Definition
*/

#include <unistd.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <ctype.h>
#include "libwebsocket/libwebsockets.h"
#include "ConfigNodes.h"
#include "XMLConfig.h"
#include "Network.h"
#include "IHex.h"
#define SERVER_INCLUDE 1
#include "../Apps/Common/mcp2515.h"



void ConfigShade(struct Node *Node, struct Node *Action, struct EEPromSensFunc *SF)
{
  int i ;
  int L1,K1,P1 ;
  unsigned char *Data2 ;
  int Command ;
  
  // Adresse setzen

  if (GetNodeAdress(Action->Data.Aktion.Unit,&L1,&K1,&P1)!=0) {
    fprintf (stderr,"Unit %s has no Adress in Sensor %s\n",Action->Data.Aktion.UnitName,Node->Name) ;
    return ;
  } ;

  SF->TargetLine = L1 ;
  SF->TargetAdd[0] = 0 ;
  SF->TargetAdd[1] = 0 ;

  if ((Action->Data.Aktion.Type==A_SHADE_UP_FULL)||(Action->Data.Aktion.Type==A_SHADE_UP_SHORT)) {
    Command = SHADE_UP_FULL ;
  } else {
    Command = SHADE_DOWN_FULL ;
  } ;

  SF->Command = Command ;

  Data2 = SF->Data ;

  // Naechsten freien Platz in der Rolladenliste suchen...
  for (i=0;i<6;i++) if (Data2[i*2]==0) break ;
  Data2[i*2] = K1 ;
  Data2[i*2+1] = P1 ; // Achtung: Hier wird das naechste SensFunc ueberschrieben, dies ist jedoch fuer Rollaeden moeglich...
}

void ConfigCommand (struct Node *Node, struct Node *Action, struct EEPromSensFunc *SF)
{
  int L1,K1,P1 ;
  int Command ;

  // Adresse setzen

  if (GetNodeAdress(Action->Data.Aktion.Unit,&L1,&K1,&P1)!=0) {
    fprintf (stderr,"Unit %s has no Adress in Sensor %s\n",Action->Data.Aktion.UnitName,Node->Name) ;
    return ;
  } ;

  SF->TargetLine = L1 ;
  SF->TargetAdd[0] = (unsigned char) (K1&0xff) ;
  SF->TargetAdd[1] = (unsigned char) ((K1>>8)&0xff) ;

  // Kommando uebersetzen...

  Command = 0 ;

  switch (Action->Data.Aktion.Type) {
  case A_ON: 
    if (Action->Data.Aktion.Unit->Type==N_ONOFF) Command = CHANNEL_ON ;
    break ;
  case A_OFF: 
    if (Action->Data.Aktion.Unit->Type==N_ONOFF) Command = CHANNEL_OFF ;
    break ;
  case A_TOGGLE: 
    if (Action->Data.Aktion.Unit->Type==N_ONOFF) Command = CHANNEL_TOGGLE ;
    break ;
  case A_SHADE_UP_FULL: 
    if (Action->Data.Aktion.Unit->Type==N_SHADE) Command = SHADE_UP_FULL ;
    break ;
  case A_SHADE_DOWN_FULL: 
    if (Action->Data.Aktion.Unit->Type==N_SHADE) Command = SHADE_DOWN_FULL ;
    break ;
  case A_SHADE_UP_SHORT: 
    if (Action->Data.Aktion.Unit->Type==N_SHADE) Command = SHADE_UP_SHORT ;
    break ;
  case A_SHADE_DOWN_SHORT: 
    if (Action->Data.Aktion.Unit->Type==N_SHADE) Command = SHADE_DOWN_SHORT ;
    break ;
  case A_SHADE_TO: 
    break ;
  case A_CALL:
    if (Action->Data.Aktion.Unit->Type==N_MACRO) Command = START_PROG ;
    break ;
  default:
    break ;
  } ;

  SF->Command = Command ;

  // Je nach Kommando jetzt noch den Datenbereich fuellen...

  if ((Command==CHANNEL_ON)||(Command==CHANNEL_OFF)||(Command==CHANNEL_TOGGLE)) {
    SF->Data[0] = P1 ;
  } else if ((Command==SHADE_UP_SHORT)||(Command==SHADE_DOWN_SHORT)||
	     (Command==SHADE_UP_FULL)||(Command==SHADE_DOWN_FULL)) {
    SF->Data[0] = P1 ;
  } else if (Command==START_PROG) {
    SF->Data[0] = P1 ;
  } else {
    // Kommando wurde nicht gesetzt
    SF->Command = UNDEFINED_COMMAND ;
  }
}

void MakeSensorConfig (struct Node *Node, struct EEPROM *EEprom)
{
  int i ;
  int L1,K1,P1 ;
  struct Node *Action ;
  struct EEPromSensFunc *Func ;

  GetNodeAdress(Node,&L1,&K1,&P1) ;

  // Konfiguration uebersetzen
  switch (Node->Data.Sensor.SensorTyp) {
  case S_SIMPLE:
    i = 1 ;
    break ;
  case S_SHORTLONG:
    i = 2 ;
    break ;
  case S_MONO:
    i = 3 ;
    break ;
  case S_RETMONO:
    i = 4 ;
    break ;
  case S_ANALOG:
    i = 5 ;
    break ;
  case S_SHADE_SHORTLONG:
    i = 2 ;
    break ;
  case S_OUTPUT:
    i = 10 ;
    break ;
  case S_WSDATA:
    i = 21 ;
    break ;
  case S_WSCLOCK:
    i = 20 ;
    break ;
  default:
    i = 1 ;
    break ;
  } ;

  // Konfiguration eintragen
  EEprom->Data.Sensor.Config[P1-1].Config = i ;
  if (i==2) {
    EEprom->Data.Sensor.REPEAT_START = Node->Data.Sensor.Lang ;
    EEprom->Data.Sensor.REPEAT_END = Node->Data.Sensor.Ende ;
  } ;
  if ((i==3)||(i==4)||(i==5)) {
    EEprom->Data.Sensor.Config[P1-1].Data = Node->Data.Sensor.Intervall ;
  }  ;
  if (i==10) {
    EEprom->Data.Sensor.Config[P1-1].Data = Node->Data.Sensor.Reset ;
  } ;

  for (Action=Node->Child;Action!=NULL;Action=Action->Next) {
    if (Action->Type!=N_ACTION) continue ; // Nur Aktionen abarbeiten
    // Bestimmen, welche Funktion konfiguriert werden soll
    if ((Node->Data.Sensor.SensorTyp==S_SIMPLE)||(Node->Data.Sensor.SensorTyp==S_SHADE_SHORTLONG)
	||(Node->Data.Sensor.SensorTyp==S_ANALOG)) Action->Data.Aktion.Short = 1 ; // Fuer die gibt es nur "Kurze" Konfigurationen
    if ((Action->Data.Aktion.StandAlone==1)&&(Action->Data.Aktion.Short==1)) Func=&(EEprom->Data.Sensor.Pin[P1-1].ShortAuto) ;
    if ((Action->Data.Aktion.StandAlone==1)&&(Action->Data.Aktion.Short==0)) Func=&(EEprom->Data.Sensor.Pin[P1-1].LongAuto) ;
    if ((Action->Data.Aktion.StandAlone==0)&&(Action->Data.Aktion.Short==1)) Func=&(EEprom->Data.Sensor.Pin[P1-1].ShortMaster) ;
    if ((Action->Data.Aktion.StandAlone==0)&&(Action->Data.Aktion.Short==0)) Func=&(EEprom->Data.Sensor.Pin[P1-1].LongAuto) ;
    if (Node->Data.Sensor.SensorTyp==S_SHADE_SHORTLONG) {
      ConfigShade(Node,Action,Func) ;
    } else if ((Node->Data.Sensor.SensorTyp==S_SIMPLE)||(Node->Data.Sensor.SensorTyp==S_SHORTLONG)||
	       (Node->Data.Sensor.SensorTyp==S_MONO)||(Node->Data.Sensor.SensorTyp==S_RETMONO)) {
      ConfigCommand(Node,Action,Func) ;
    } else {
      // Analog-Konfiguration einfuegen
    } ;
  }
}

void MakeRolloConfig (struct Node *Node, struct EEPROM *EEprom)
{
  int L1,K1,P1 ;
  GetNodeAdress(Node,&L1,&K1,&P1) ;

  EEprom->Data.Relais.RTFull[P1-1] = Node->Data.Rollo.Lang ;
  EEprom->Data.Relais.RTShort[P1-1] = Node->Data.Rollo.Kurz ;
  EEprom->Data.Relais.UpDown[P1-1] = Node->Data.Rollo.Swap ;
}

int MakeConfig (int Linie, int Knoten, struct EEPROM *EEprom)
{
  int i,j;
  int L1,K1,P1 ;
  int L2,K2,P2 ;
  struct Node *ANodes[MAX_ADD_PER_NODE*4] ;
  int ANumber ;

  ANumber = 0 ;

  // Konfiguration loeschen
  EEprom->Magic[0] = 0xBA ;
  EEprom->Magic[1] = 0xCA ;
  EEprom->BoardAdd[0] = (unsigned char) (Knoten&0xff) ;
  EEprom->BoardAdd[1] = (unsigned char) ((Knoten>>8)&0xff) ;
  EEprom->BoardLine = Linie; 
  EEprom->BootAdd[0] = 0x01 ;
  EEprom->BootAdd[1] = 0x00 ;
  EEprom->BootLine = 0x00 ;
  EEprom->PAD = 0xFF ;
  for (i=0;i<502;i++) EEprom->Data.PAD[i] = 0x00 ;


  CollectAdress(Haus,Linie,Knoten,ANodes,&ANumber) ;

  if (ANumber==0) return(0) ; // Keinen Knoten mit der Adresse gefunden

  // Ueberpruefen, ob Konfiguration konsistent ist

  for (i=0;i<ANumber-1;i++) {
    GetNodeAdress(ANodes[i],&L1,&K1,&P1) ;
    for (j=i+1;j<ANumber;j++) {
      GetNodeAdress(ANodes[j],&L2,&K2,&P2) ;
      if (P1==P2) {
	fprintf (stderr,"Config: Port defined twice for L:%d, K:%d : in %s and %s\n",Linie,Knoten,ANodes[i]->Name,ANodes[j]->Name) ;
	return(0) ; // Konfiguration nicht konsistent
      } ;
    } ;
  } ;
  
  EEprom->BoardType=0xFF ;
  
  for (i=0;i<ANumber;i++) {
    switch (ANodes[i]->Type) {
    case N_SENSOR:
      if (EEprom->BoardType==0xFF) {
	EEprom->BoardType = 32 ;
      } else {
	if (EEprom->BoardType!=32) {
	  fprintf (stderr,"Inconsistent board L:%d, K:%d definition for sensor %s\n",Linie,Knoten,ANodes[i]->Name) ;
	  return (0) ;
	};
      } ;
      MakeSensorConfig(ANodes[i],EEprom) ;
      break ;
    case N_SHADE:
      if (EEprom->BoardType==0xFF) {
	EEprom->BoardType = 16 ;
      } else {
	if (EEprom->BoardType!=16) {
	  fprintf (stderr,"Inconsistent board L:%d, K:%d definition for sensor %s\n",Linie,Knoten,ANodes[i]->Name) ;
	  return (0) ;
	};
      } ;
      MakeRolloConfig(ANodes[i],EEprom) ;
      break ;
    default:
      break ;
    } ;
  } ;
  return (1) ;
}

int WriteConfig(struct EEPROM *EEprom)
{
  int Line, Add ;
  FILE *OutFile ;
  unsigned char *FileBuffer ;
  int i ;
  IHexRecord IR ;
  char FileName[NAMELEN] ;

  Line = EEprom->BoardLine ;
  Add = (EEprom->BoardAdd[1]<<8)+EEprom->BoardAdd[0] ;
  FileBuffer = (unsigned char*) EEprom ;

  sprintf (FileName,"NodeConf\\Config_%d_%d.eep",(int)Line,(int)Add) ;
  
  OutFile = fopen(FileName,"w") ;

  if (OutFile==NULL) {
    fprintf (stderr,"Konnte Konfiguration nicht sichern\n") ;
    return (-1) ;
  } ;
  for (i=0;i<16;i++) { /* 16 Records schreiben */
    New_IHexRecord(IHEX_TYPE_00,i*32,&(FileBuffer[i*32]),32,&IR) ;
    Write_IHexRecord(&IR,OutFile); 
  } ;
  New_IHexRecord (IHEX_TYPE_01,0,FileBuffer,0,&IR) ;
  Write_IHexRecord(&IR,OutFile); 
  fclose (OutFile) ;
  return (1) ;
}

// Sendet das naechste Byte der Konfiguration an den Knoten, 
struct ListItem *ConfigList ;
struct ListItem *FirmwareList ;

void SendConfigByte (char Linie, USHORT Knoten)
{
  struct ListItem *Config ;
  USHORT K1 ;
  char L1 ;
  ULONG CANID ;
  unsigned char Data[8]; 
  char Len ;

  // Configuration suchen
  for (Config = ConfigList;Config!=NULL;Config=Config->Next) {
    L1 = Config->Data.EEprom.BoardLine ;
    K1 = (Config->Data.EEprom.BoardAdd[1]<<8)+Config->Data.EEprom.BoardAdd[0] ;
    if ((L1==Linie)&&(K1==Knoten)) break ;
  } ;

  if (Config==NULL) {
    fprintf (stderr,"Config byte acknowledged from Line:%d, Node:%d, without beeing sent!\n",Linie,Knoten) ;
    return ;
  } ;

  if (Config->Counter>=sizeof(struct EEPROM)) {
    // das letzte Byte wurde best�tigt, nun noch den Knoten zur�cksetzen um die Konfiguration zu laden.
    CANID = BuildCANId(0,0,0,2,Linie,Knoten,0) ;
    Data[0] = START_BOOT ;
    Data[1] = 0 ;
    Len = 1 ;
    SendCANMessage(CANID,Len,Data) ;
    // aus der Liste der abzuarbeitenden Sachen entfernen
    if (Config==ConfigList) ConfigList=Config->Next ;
    FreeItem(Config) ;
    return ;
  } ;

  // Naechstes Byte senden
  CANID = BuildCANId(0,0,0,2,Linie,Knoten,0) ;
  Data[0] = SET_VAR ;
  Data[1] = (unsigned char)(Config->Counter&0xFF) ;
  Data[2] = (unsigned char)(Config->Counter>>8) ;
  Data[3] = Config->Data.Command[Config->Counter] ;
  Len=4 ;
  SendCANMessage(CANID,Len,Data) ;
  
  // Auf naechstes Byte setzen
  Config->Counter++ ;
}

void SendConfig(struct EEPROM *EEprom)
{
  struct ListItem *Config ;
  USHORT K1 ;
  char L1 ;
  
  L1 = EEprom->BoardLine ;
  K1 = (EEprom->BoardAdd[1]<<8)+EEprom->BoardAdd[0] ;
  
  Config = CreateItem(ConfigList) ;
  if (ConfigList==NULL) ConfigList = Config ;

  // Die ersten zehn Bytes sind die Adress-Informationen, diese nicht aendern...
  Config->Counter = 10 ;
  Config->Data.EEprom = *EEprom ;

  // Das erste Byte senden, dieses triggert dann alle weiteren
  SendConfigByte(L1,K1) ;
}

void SendFirmware(char Linie, USHORT Knoten)
{
  struct ListItem *Firmware ;
  int i ;
  int FileSize ;
  int TypCode ;
  struct Node *FNodes[MAX_ADD_PER_NODE*4] ;
  int FNumber ;
  struct Node *ANodes[MAX_ADD_PER_NODE*4] ;
  int ANumber ;
  ULONG CANID ;
  unsigned char Data[8]; 
  char Len ;

  
  // Neuen Update-Request initiieren
  Firmware = CreateItem(FirmwareList) ;
  if (FirmwareList==NULL) FirmwareList = Firmware ;

  // Typnummer des Knotens ermitteln
  ANumber = 0 ;
  CollectAdress(Haus,Linie,Knoten,ANodes,&ANumber) ;
  
  TypCode = 0 ;
  switch (ANodes[0]->Type) {
  case N_ONOFF:
  case N_SHADE:
    TypCode = 16 ;
    break ;
  case N_SENSOR:
    TypCode = 32 ;
    break ;
  default:
    TypCode = 255 ;
  } ;

  FNumber = 0 ;
  // Alle Firmwaren suchen
  CollectType (Haus,N_FIRMWARE,FNodes,&FNumber) ;

  for (i=0;i<FNumber;i++) if (FNodes[i]->Value==TypCode) break ;
  
  if (i==FNumber) {
    // Fuer den Typ gibt es keine Firmware
    return ;
  } ;

  // Firmware laden
  FileSize = LoadIHexFile(FNodes[i]->Name,0) ;
  if (FileSize<=0) {
    // Fuer den Typ gibt es keine Firmware
    return ;
  }

  Firmware->Data.Code = malloc (sizeof(char)*(FileSize+256)) ;

  if (Firmware->Data.Code==NULL) {
    fprintf (stderr,"OutOfMemory in SendFirmware !\n") ;
    exit(1) ;
  }
  
  // Geladene Firware in den Puffer uebertragen
  
  for (i=0;i<FileSize;i++) Firmware->Data.Code[i]=FileBuffer[i] ;
  for (;i<FileSize+255;i++) Firmware->Data.Code[i] = 0xFF ;

  Firmware->Number = FileSize ;
  Firmware->Counter= 0 ;
  Firmware->Linie = Linie ;
  Firmware->Knoten = Knoten ;
  Firmware->State = 0 ;
  Firmware->Package = 0 ;

  // Reset des Knotens loest Anfrage nach Firmwareupdate aus...

  CANID = BuildCANId(0,0,0,2,Linie,Knoten,0) ;
  Data[0] = START_BOOT ;
  Data[1] = 0 ;
  Len = 1 ;
  SendCANMessage(CANID,Len,Data) ;
}

void SendFirmwareByte (char Linie, USHORT Knoten,unsigned char *Response, char ResponseLen) 
{
  struct ListItem *Firmware ;
  ULONG CANID ;
  unsigned char Data[8]; 
  int PageSize ;
  char Len ;
  
  // Den entsprechenden Update-Buffer suchen
  for (Firmware=FirmwareList;Firmware!=NULL;Firmware=Firmware->Next) if ((Firmware->Linie==Linie)&&(Firmware->Knoten==Knoten)) break ;  
 
  if (Firmware==NULL) return ; // Es steht kein Update an...

  CANID = BuildCANId(0,0,0,2,Linie,Knoten,0) ;
  
  if (((ResponseLen==6)&&(Response[0]==UPDATE_REQ))||
      (Response[0]==WRONG_NUMBER_RESPONSE)||
      ((Response[0]&WRONG_NUMBER_RESPONSE)!=SUCCESSFULL_RESPONSE)) {

    // Wenn ein Update ansteht oder ein Fehler aufgetreten ist, dann initialisieren
    Firmware->State = 1 ;
    Firmware->Package = 0 ;
    Data[0] = IDENTIFY ;
    Data[1] = Firmware->Package++;
    Len = 2 ;
    SendCANMessage(CANID,Len,Data) ;
    return ;
  } ;

  if (Firmware->State==0) return; // Kommunikation ist schief gelaufen...

  if (Firmware->State==1) {

    // Laenge der Datei an die Pagesize anpassen...
    PageSize = 32<<Response[3] ;
    if (Firmware->Number%PageSize!=0) Firmware->Number = ((Firmware->Number/PageSize)+1)*PageSize ;
    Firmware->State = 2 ;
    Firmware->Counter = 0 ;

    // Startadresse auf "0" setzen
    Data[0] = SET_ADDRESS ;
    Data[1] = Firmware->Package++ ;
    Data[2] = 0 ;
    Data[3] = 0 ;
    Data[4] = 0 ;
    Data[5] = 0 ;
    Len = 6 ;
    SendCANMessage(CANID,Len,Data) ;
    return ;
  } ;
  
  if (Firmware->State==2) {

    // Daten in 4er-Bloecken uebertragen
    Data[0] = DATA ;
    Data[1] = Firmware->Package++ ;
    Data[2] = Firmware->Data.Code[Firmware->Counter] ;
    Data[3] = Firmware->Data.Code[Firmware->Counter+1] ;
    Data[4] = Firmware->Data.Code[Firmware->Counter+2] ;
    Data[5] = Firmware->Data.Code[Firmware->Counter+3] ;
    Len = 6 ;
    SendCANMessage(CANID,Len,Data) ;
    Firmware->Counter+=4 ;
    if (Firmware->Counter>=Firmware->Number) Firmware->State=3 ;
    return ;
  } ;

  if (Firmware->State==3) {

    // Alle Daten uebertragen, die Applikation starten...
    Data[0] = START_APP ;
    Data[1] = Firmware->Package++ ;
    Len = 2 ;
    SendCANMessage(CANID,Len,Data) ;
    Firmware->State=4 ;
    return ;
  } ;

  if (Firmware->State==4) {

    // Update abgeschlossen, Firmware aus der Liste loeschen...
    if (Firmware==FirmwareList) FirmwareList=Firmware->Next ;
    // Geladene Firmware freigeben
    free(Firmware->Data.Code) ;
    FreeItem(Firmware) ;
    return ;
  } ;
}
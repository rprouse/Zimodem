/*
   Copyright 2016-2019 Bo Zimmerman

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

     http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/

void ZConfig::switchTo()
{
  currMode=&configMode;
  serial.setFlowControlType(commandMode.serial.getFlowControlType());
  serial.setPetsciiMode(commandMode.serial.isPetsciiMode());
  savedEcho=commandMode.doEcho;
  newListen=commandMode.preserveListeners;
  commandMode.doEcho=true;
  serverSpec.port=6502;
  if(servs)
    serverSpec = *servs;
  serial.setXON(true);
  showMenu=true;
  EOLN=commandMode.EOLN;
  EOLNC=EOLN.c_str();
  currState = ZCFGMENU_MAIN;
  lastNumber=0;
  lastAddress="";
  lastOptions="";
  settingsChanged=false;
  lastNumNetworks=0;
}

void ZConfig::serialIncoming()
{
  bool crReceived=commandMode.readSerialStream();
  commandMode.clearPlusProgress(); // re-check the plus-escape mode
  if(crReceived)
  {
    doModeCommand();
  }
}

void ZConfig::switchBackToCommandMode()
{
  commandMode.doEcho=savedEcho;
  currMode = &commandMode;
}

void ZConfig::doModeCommand()
{
  String cmd = commandMode.getNextSerialCommand();
  char c='?';
  for(int i=0;i<cmd.length();i++)
  {
    if(cmd[i]>32)
    {
      c=lc(cmd[i]);
      break;
    }
  }
  switch(currState)
  {
    case ZCFGMENU_MAIN:
    {
      if((c=='q')||(cmd.length()==0))
      {
        if(settingsChanged)
        {
          currState=ZCFGMENU_WICONFIRM;
          showMenu=true;
        }
        else
        {
          commandMode.showInitMessage();
          switchBackToCommandMode();
          return;
        }
      }
      else
      if(c=='a') // add to phonebook
      {
        currState=ZCFGMENU_NUM;
        showMenu=true;
      }
      else
      if(c=='w') // wifi
      {
        currState=ZCFGMENU_WIMENU;
        showMenu=true;
      }
      else
      if(c=='h') // host
      {
        currState=ZCFGMENU_NEWHOST;
        showMenu=true;
      }
      else
      if(c=='f') // flow control
      {
        currState=ZCFGMENU_FLOW;
        showMenu=true;
      }
      else
      if(c=='p') // petscii translation toggle
      {
        commandMode.serial.setPetsciiMode(!commandMode.serial.isPetsciiMode());
        serial.setPetsciiMode(commandMode.serial.isPetsciiMode());
        settingsChanged=true;
        showMenu=true;
      }
      else
      if(c=='e') // echo
      {
        savedEcho = !savedEcho;
        settingsChanged=true;
        showMenu=true;
      }
      else
      if(c=='b') // bbs
      {
        currState=ZCFGMENU_BBSMENU;
        showMenu=true;
      }
      else
      if(c>47 && c<58) // its a phonebook entry!
      {
        PhoneBookEntry *pb=PhoneBookEntry::findPhonebookEntry(cmd);
        if(pb == null)
        {
          serial.printf("%s%sPhone number not found: '%s'.%s%s",EOLNC,EOLNC,cmd.c_str(),EOLNC,EOLNC);
          currState=ZCFGMENU_MAIN;
          showMenu=true;
        }
        else
        {
          lastNumber = pb->number;
          lastAddress = pb->address;
          lastOptions = pb->modifiers;
          currState=ZCFGMENU_ADDRESS;
          showMenu=true;
        }
      }
      else
      {
        showMenu=true; // re-show the menu
      }
      break;
    }
    case ZCFGMENU_WICONFIRM:
    {
      if((cmd.length()==0)||(c=='n'))
      {
        commandMode.showInitMessage();
        switchBackToCommandMode();
        return;
      }
      else
      if(c=='y')
      {
        if(newListen != commandMode.preserveListeners)
        {
          commandMode.preserveListeners=newListen;
          if(!newListen)
          {
            SPIFFS.remove("/zlisteners.txt");
            WiFiServerNode::DestroyAllServers();
          }
          else
          {
            commandMode.ringCounter=1;
            commandMode.autoStreamMode=true;
            WiFiServerNode *s=WiFiServerNode::FindServer(serverSpec.port);
            if(s != null)
              delete s;
            s = new WiFiServerNode(serverSpec.port,serverSpec.flagsBitmap);
            WiFiServerNode::SaveWiFiServers();
          }
        }
        else
        if(commandMode.preserveListeners)
        {
          WiFiServerNode *s = WiFiServerNode::FindServer(serverSpec.port);
          if( s != null)
          {
            if(s->flagsBitmap != serverSpec.flagsBitmap)
            {
              s->flagsBitmap = serverSpec.flagsBitmap;
              WiFiServerNode::SaveWiFiServers();
            }
          }
          else
          {
            WiFiServerNode::DestroyAllServers();
            s = new WiFiServerNode(serverSpec.port,serverSpec.flagsBitmap);
            WiFiServerNode::SaveWiFiServers();
          }
        }
        commandMode.reSaveConfig();
        serial.printf("%sSettings saved.%s",EOLNC,EOLNC);
        commandMode.showInitMessage();
        switchBackToCommandMode();
        return;
      }
      else
        showMenu=true;
    }
    case ZCFGMENU_NUM:
    {
      PhoneBookEntry *pb=PhoneBookEntry::findPhonebookEntry(cmd);
      if(pb != null)
      {
        serial.printf("%s%sNumber already exists '%s'.%s%s",EOLNC,EOLNC,cmd.c_str(),EOLNC,EOLNC);
        currState=ZCFGMENU_MAIN;
        showMenu=true;
      }
      else
      {
        lastNumber = atol((char *)cmd.c_str());
        lastAddress = "";
        lastOptions = "";
        currState=ZCFGMENU_ADDRESS;
        showMenu=true;
      }
      break;
    }
    case ZCFGMENU_NEWPORT:
    {
      if(cmd.length()>0)
      {
        serverSpec.port = atoi((char *)cmd.c_str());
        settingsChanged=true;
      }
      currState=ZCFGMENU_BBSMENU;
      showMenu=true;
      break;
    }
    case ZCFGMENU_ADDRESS:
    {
      PhoneBookEntry *entry = PhoneBookEntry::findPhonebookEntry(lastNumber);
      if(cmd.equalsIgnoreCase("delete") && (entry != null))
      {
        delete entry;
        currState=ZCFGMENU_MAIN;
        serial.printf("%sPhonebook entry deleted.%s%s",EOLNC,EOLNC,EOLNC);
      }
      else
      if((cmd.length()==0) && (entry != null))
          currState=ZCFGMENU_OPTIONS; // just keep old values
      else
      {
        boolean fail = cmd.indexOf(',') >= 0;
        int colonDex=cmd.indexOf(':');
        fail = fail || (colonDex <= 0) || (colonDex == cmd.length()-1);
        fail = fail || (colonDex != cmd.lastIndexOf(':'));
        if(!fail)
        {
          for(int i=colonDex+1;i<cmd.length();i++)
            if(strchr("0123456789",cmd[i])<0)
              fail=true;
        }
        if(fail)
        {
          serial.printf("%sInvalid address format (hostname:port) for '%s'.%s%s",EOLNC,cmd.c_str(),EOLNC,EOLNC);
        }
        else
        {
          lastAddress = cmd;
          currState=ZCFGMENU_OPTIONS;
        }
      }
      showMenu=true; // re-show the menu
      break;
    }
    case ZCFGMENU_BBSMENU:
    {
      if(cmd.length()==0)
        currState=ZCFGMENU_MAIN;
      else
      {
        ConnSettings flags(serverSpec.flagsBitmap);
        switch(c)
        {
        case 'h':
          currState=ZCFGMENU_NEWPORT;
          break;
        case 'd':
          newListen=false;
          settingsChanged=true;
          break;
        case 'p': 
          if(newListen)
          {
            flags.petscii=!flags.petscii;
            settingsChanged=true;
          }
          break;
        case 't': 
          if(newListen)
          {
            flags.telnet=!flags.telnet;
            settingsChanged=true;
          }
          break;
        case 'e':
          if(newListen)
            flags.echo=!flags.echo;
          else
            newListen=true;
          settingsChanged=true;
          break;
        case 'f':
          if(newListen)
          {
            if(flags.xonxoff)
            {
              flags.xonxoff=false;
              flags.rtscts=true; 
            }
            else
            if(flags.rtscts)
              flags.rtscts=false;
            else
              flags.xonxoff=true;
            settingsChanged=true;
          }
          break;
        case 's': 
          if(newListen)
          {
            flags.secure=!flags.secure;
            settingsChanged=true;
          }
          break;
         
        default:
          serial.printf("%sInvalid option '%s'.%s%s",EOLNC,cmd.c_str(),EOLNC,EOLNC);
          break;
        }
        settingsChanged=true;
        serverSpec.flagsBitmap = flags.getBitmap();
      }
      showMenu=true;
      break;
    }
    case ZCFGMENU_OPTIONS:
    {
      if(cmd.length()==0)
      {
        PhoneBookEntry *entry = PhoneBookEntry::findPhonebookEntry(lastNumber);
        if(entry != null)
        {
          serial.printf("%sPhonebook entry updated.%s%s",EOLNC,EOLNC,EOLNC);
          delete entry;
        }
        else
          serial.printf("%sPhonebook entry added.%s%s",EOLNC,EOLNC,EOLNC);
        entry = new PhoneBookEntry(lastNumber,lastAddress.c_str(),lastOptions.c_str());
        PhoneBookEntry::savePhonebook();
        currState=ZCFGMENU_MAIN;
      }
      else
      {
        ConnSettings flags(lastOptions.c_str());
        switch(c)
        {
          case 'p': 
            flags.petscii=!flags.petscii;
            break;
          case 't': 
            flags.telnet=!flags.telnet;
            break;
          case 'e': 
            flags.echo=!flags.echo;
            break;
          case 'f':
            if(flags.xonxoff)
            {
              flags.xonxoff=false;
              flags.rtscts=true; 
            }
            else
            if(flags.rtscts)
              flags.rtscts=false;
            else
              flags.xonxoff=true;
            break;
          case 's': 
            flags.secure=!flags.secure;
            break;
          default:
            serial.printf("%sInvalid toggle option '%s'.%s%s",EOLNC,cmd.c_str(),EOLNC,EOLNC);
            break;
        }
        lastOptions = flags.getFlagString();
      }
      showMenu=true; // re-show the menu
      break;
    }
    case ZCFGMENU_WIMENU:
    {
      if(cmd.length()==0)
      {
        currState=ZCFGMENU_MAIN;
        showMenu=true;
      }
      else
      {
        int num=atoi(cmd.c_str());
        if((num<=0)||(num>lastNumNetworks))
            serial.printf("%sInvalid number.  Try again.%s",EOLNC,EOLNC);
        else
        if(WiFi.encryptionType(num-1) == ENC_TYPE_NONE)
        {
          if(!connectWifi(WiFi.SSID(num-1).c_str(),""))
          {
            serial.printf("%sUnable to connect to %s. :(%s",EOLNC,WiFi.SSID(num-1).c_str(),EOLNC);
          }
          else
          {
            wifiSSI=WiFi.SSID(num-1);
            wifiPW="";
            settingsChanged=true;
            serial.printf("%sConnected!%s",EOLNC,EOLNC);
            currState=ZCFGMENU_MAIN;
          }
          showMenu=true;
        }
        else
        {
          lastNumber=num-1;
          currState=ZCFGMENU_WIFIPW;
          showMenu=true;
        }
      }
      break;
    }
    case ZCFGMENU_NEWHOST:
      if(cmd.length()==0)
        currState=ZCFGMENU_WIMENU;
      else
      {
        hostname=cmd;
        hostname.replace(',','.');
        if((wifiSSI.length() > 0) && (WiFi.status()==WL_CONNECTED))
        {
            if(!connectWifi(wifiSSI.c_str(),wifiPW.c_str()))
              serial.printf("%sUnable to connect to %s. :(%s",EOLNC,wifiSSI.c_str(),EOLNC);
            settingsChanged=true;
        }
        currState=ZCFGMENU_MAIN;
        showMenu=true;
      }
      break;
    case ZCFGMENU_WIFIPW:
      if(cmd.length()==0)
      {
        currState=ZCFGMENU_WIMENU;
        showMenu=true;
      }
      else
      {
          if(!connectWifi(WiFi.SSID(lastNumber).c_str(),cmd.c_str()))
          {
            serial.printf("%sUnable to connect to %s. :(%s",EOLNC,WiFi.SSID(lastNumber).c_str(),EOLNC);
          }
          else
          {
            wifiSSI=WiFi.SSID(lastNumber);
            wifiPW=cmd;
            settingsChanged=true;
            serial.printf("%sConnected!%s",EOLNC,EOLNC);
            currState=ZCFGMENU_MAIN;
          }
          showMenu=true;
      }
      break;
    case ZCFGMENU_FLOW:
      if(cmd.length()==0)
      {
        currState=ZCFGMENU_WIMENU;
        showMenu=true;
      }
      else
      {
        currState=ZCFGMENU_MAIN;
        showMenu=true;
        if(c=='x')
          commandMode.serial.setFlowControlType(FCT_NORMAL);
        else
        if(c=='r')
          commandMode.serial.setFlowControlType(FCT_RTSCTS);
        else
        if(c=='d')
          commandMode.serial.setFlowControlType(FCT_DISABLED);
        else
        {
          serial.printf("%sUnknown flow control type '%s'.  Try again.%s",EOLNC,cmd.c_str(),EOLNC);
          currState=ZCFGMENU_FLOW;
        }
        settingsChanged = settingsChanged || (currState ==ZCFGMENU_MAIN);
        serial.setFlowControlType(commandMode.serial.getFlowControlType());
        serial.setXON(true);
      }
      break;
  }
}

void ZConfig::loop()
{
  if(showMenu)
  {
    showMenu=false;
    switch(currState)
    {
      case ZCFGMENU_MAIN:
      {
        serial.printf("%sMain Menu%s",EOLNC,EOLNC);
        serial.printf("[HOST] name: %s%s",hostname.c_str(),EOLNC);
        serial.printf("[WIFI] connection: %s%s",(WiFi.status() == WL_CONNECTED)?wifiSSI.c_str():"Not connected",EOLNC);
        String flowName;
        switch(commandMode.serial.getFlowControlType())
        {
          case FCT_NORMAL:
            flowName = "XON/XOFF";
            break;
          case FCT_RTSCTS:
            flowName = "RTS/CTS";
            break;
          case FCT_DISABLED:
            flowName = "DISABLED";
            break;
          default:
            flowName = "OTHER";
            break;
        }
        String bbsMode = "OFF";
        if(newListen)
        {
          bbsMode = "Port ";
          bbsMode += serverSpec.port;
        }
        serial.printf("[FLOW] control: %s%s",flowName.c_str(),EOLNC);
        serial.printf("[ECHO] keystrokes: %s%s",savedEcho?"ON":"OFF",EOLNC);
        serial.printf("[BBS] host: %s%s",bbsMode.c_str(),EOLNC);
        serial.printf("[PETSCII] translation: %s%s",commandMode.serial.isPetsciiMode()?"ON":"OFF",EOLNC);
        serial.printf("[ADD] new phonebook entry%s",EOLNC);
        PhoneBookEntry *p = phonebook;
        if(p != null)
        {
          serial.printf("Phonebook entries:%s",EOLNC);
          while(p != null)
          {
            serial.printf("  [%lu] %s%s",p->number, p->address, EOLNC);
            p=p->next;
          }
        }
        serial.printf("%sEnter command or entry or ENTER to exit: ",EOLNC,EOLNC);
        break;
      }
      case ZCFGMENU_NUM:
        serial.printf("%sEnter a new fake phone number (digits ONLY)%s: ",EOLNC,EOLNC);
        break;
      case ZCFGMENU_NEWPORT:
        serial.printf("%sEnter a port number to listen on%s: ",EOLNC,EOLNC);
        break;
      case ZCFGMENU_ADDRESS:
      {
        PhoneBookEntry *lastEntry = PhoneBookEntry::findPhonebookEntry(lastNumber);
        if(lastEntry == null)
          serial.printf("%sEnter a new hostname:port%s: ",EOLNC,EOLNC);
        else
          serial.printf("%sModify hostname:port, or enter DELETE (%s)%s: ",EOLNC,lastAddress.c_str(),EOLNC);
        break;
      }
      case ZCFGMENU_OPTIONS:
      {
        ConnSettings flags(lastOptions.c_str());
        serial.printf("%sConnection Options:%s",EOLNC,EOLNC);
        serial.printf("[PETSCII] Translation: %s%s",flags.petscii?"ON":"OFF",EOLNC);
        serial.printf("[TELNET] Translation: %s%s",flags.telnet?"ON":"OFF",EOLNC);
        serial.printf("[ECHO]: %s%s",flags.echo?"ON":"OFF",EOLNC);
        serial.printf("[FLOW] Control: %s%s",flags.xonxoff?"XON/XOFF":flags.rtscts?"RTS/CTS":"DISABLED",EOLNC);
        serial.printf("%sEnter option to toggle or ENTER to exit%s: ",EOLNC,EOLNC);
        break;
      }
      case ZCFGMENU_BBSMENU:
      {
        serial.printf("%sBBS host settings:%s",EOLNC,EOLNC);
        if(newListen)
        {
          ConnSettings flags(serverSpec.flagsBitmap);
          serial.printf("%s[HOST] Listener Port: %d%s",EOLNC,serverSpec.port,EOLNC);
          serial.printf("[PETSCII] Translation: %s%s",flags.petscii?"ON":"OFF",EOLNC);
          serial.printf("[TELNET] Translation: %s%s",flags.telnet?"ON":"OFF",EOLNC);
          serial.printf("[ECHO]: %s%s",flags.echo?"ON":"OFF",EOLNC);
          serial.printf("[FLOW] Control: %s%s",flags.xonxoff?"XON/XOFF":flags.rtscts?"RTS/CTS":"DISABLED",EOLNC);
          serial.printf("[DISABLE] BBS host listener%s",EOLNC);
        }
        else
          serial.printf("%s[ENABLE] BBS host listener%s",EOLNC,EOLNC);
        serial.printf("%sEnter option to toggle or ENTER to exit%s: ",EOLNC,EOLNC);
        break;
      }
      case ZCFGMENU_WIMENU:
      {
        int n = WiFi.scanNetworks();
        if(n>20)
          n=20;
        serial.printf("%sWiFi Networks:%s",EOLNC,EOLNC);
        lastNumNetworks=n;
        for (int i = 0; i < n; ++i)
        {
          serial.printf("[%d] ",(i+1));
          serial.prints(WiFi.SSID(i).c_str());
          serial.prints(" (");
          serial.printi(WiFi.RSSI(i));
          serial.prints(")");
          serial.prints((WiFi.encryptionType(i) == ENC_TYPE_NONE)?" ":"*");
          serial.prints(EOLN.c_str());
          serial.flush();
          delay(10);
        }
        serial.printf("%sEnter number to connect, or ENTER: ",EOLNC);
        break;
      }
      case ZCFGMENU_NEWHOST:
      {
        serial.printf("%sEnter a new hostname: ",EOLNC);
        break;
      }
      case ZCFGMENU_WIFIPW:
      {
        serial.printf("%sEnter your WiFi Password: ",EOLNC);
        break;
      }
      case ZCFGMENU_FLOW:
      {
        serial.printf("%sEnter RTS/CTS, XON/XOFF, or DISABLE flow control%s: ",EOLNC,EOLNC);
        break;
      }
      case ZCFGMENU_WICONFIRM:
      {
        serial.printf("%sYour setting changed.  Save them (y/N)?",EOLNC);
        break;
      }
    }
  }
  if(commandMode.checkPlusEscape())
  {
    switchBackToCommandMode();
  }
  else
  if(serial.isSerialOut())
  {
    serialOutDeque();
  }
}


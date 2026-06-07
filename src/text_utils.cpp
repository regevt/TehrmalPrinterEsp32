#include "text_utils.h"

String transliterate(String text) {
  String result = "";
  
  for (int i = 0; i < text.length(); i++) {
    char c = text[i];
    
    if ((c & 0xE0) == 0xC0 && i + 1 < text.length()) {
      unsigned char c1 = text[i];
      unsigned char c2 = text[i + 1];
      unsigned int unicode = ((c1 & 0x1F) << 6) | (c2 & 0x3F);
      
      switch (unicode) {
        case 0x430: result += "a"; i++; break;
        case 0x431: result += "b"; i++; break;
        case 0x432: result += "v"; i++; break;
        case 0x433: result += "g"; i++; break;
        case 0x434: result += "d"; i++; break;
        case 0x435: result += "e"; i++; break;
        case 0x451: result += "e"; i++; break;
        case 0x436: result += "zh"; i++; break;
        case 0x437: result += "z"; i++; break;
        case 0x438: result += "i"; i++; break;
        case 0x439: result += "y"; i++; break;
        case 0x43A: result += "k"; i++; break;
        case 0x43B: result += "l"; i++; break;
        case 0x43C: result += "m"; i++; break;
        case 0x43D: result += "n"; i++; break;
        case 0x43E: result += "o"; i++; break;
        case 0x43F: result += "p"; i++; break;
        case 0x440: result += "r"; i++; break;
        case 0x441: result += "s"; i++; break;
        case 0x442: result += "t"; i++; break;
        case 0x443: result += "u"; i++; break;
        case 0x444: result += "f"; i++; break;
        case 0x445: result += "kh"; i++; break;
        case 0x446: result += "ts"; i++; break;
        case 0x447: result += "ch"; i++; break;
        case 0x448: result += "sh"; i++; break;
        case 0x449: result += "shch"; i++; break;
        case 0x44A: result += ""; i++; break;
        case 0x44B: result += "y"; i++; break;
        case 0x44C: result += ""; i++; break;
        case 0x44D: result += "e"; i++; break;
        case 0x44E: result += "yu"; i++; break;
        case 0x44F: result += "ya"; i++; break;
        case 0x410: result += "A"; i++; break;
        case 0x411: result += "B"; i++; break;
        case 0x412: result += "V"; i++; break;
        case 0x413: result += "G"; i++; break;
        case 0x414: result += "D"; i++; break;
        case 0x415: result += "E"; i++; break;
        case 0x401: result += "E"; i++; break;
        case 0x416: result += "Zh"; i++; break;
        case 0x417: result += "Z"; i++; break;
        case 0x418: result += "I"; i++; break;
        case 0x419: result += "Y"; i++; break;
        case 0x41A: result += "K"; i++; break;
        case 0x41B: result += "L"; i++; break;
        case 0x41C: result += "M"; i++; break;
        case 0x41D: result += "N"; i++; break;
        case 0x41E: result += "O"; i++; break;
        case 0x41F: result += "P"; i++; break;
        case 0x420: result += "R"; i++; break;
        case 0x421: result += "S"; i++; break;
        case 0x422: result += "T"; i++; break;
        case 0x423: result += "U"; i++; break;
        case 0x424: result += "F"; i++; break;
        case 0x425: result += "Kh"; i++; break;
        case 0x426: result += "Ts"; i++; break;
        case 0x427: result += "Ch"; i++; break;
        case 0x428: result += "Sh"; i++; break;
        case 0x429: result += "Shch"; i++; break;
        case 0x42A: result += ""; i++; break;
        case 0x42B: result += "Y"; i++; break;
        case 0x42C: result += ""; i++; break;
        case 0x42D: result += "E"; i++; break;
        case 0x42E: result += "Yu"; i++; break;
        case 0x42F: result += "Ya"; i++; break;
        default:
          result += "?";
          i++;
          break;
      }
    } else {
      result += c;
    }
  }
  
  return result;
}

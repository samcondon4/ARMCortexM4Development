/******************************************************************************************
 * BasicIO.c - is a module with public functions used to send and receive
 * information from a serial port. In this case UART2 configured for the
 * Segger debug USB serial port. K65TWR board.
 * v1.1
 *  Created by: Todd Morton, 10/09/2014
 *  With Contributions by: Jacob Gilbert And Adam Slater
 * v2.1
 *  Created by Todd Morton
 *  Contributions to BIOGetStrg() by Chance Eldridge
 * v2.2
 *  Created by Todd Morton with contribution from Daniel Kowalski, 04/05/2017
 *  Added rate parameter to BIOOpen()
 *
 * v3.1
 *  Created by Todd Morton
 *  Deprecated includes.h. Replaced with MCUType.h
 * v4.1
 *  Created by Todd Morton
 *  Modified for MCUXpresso header file macros
 *******************************************************************************************
* Project master header file
********************************************************************/
#include "MCUType.h"
#include "BasicIO.h"

/*******************************************************************************************
* Private Resources
*******************************************************************************************/
static INT8C bioHtoA(INT8U hnib);   //Convert nibble to ascii
static INT8U bioIsHex(INT8C c);
static INT8U bioHtoB(INT8C c);
/*******************************************************************************************
 * void BIOOpen(INT8U rate) - Initializes UART to operate at a specified rate.
 * MCU: K65, UART2 configured for debugger USB.
 * Acceptable rates:
 *  BIO_BIT_RATE_9600
 *  BIO_BIT_RATE_19200
 *  BIO_BIT_RATE_38400
 *  BIO_BIT_RATE_57600
 *  BIO_BIT_RATE_115200
 ******************************************************************************************/
void BIOOpen(INT8U rate){

    SIM->SCGC5 |= SIM_SCGC5_PORTE(1); /* Enable clock gate for PORTE */
    SIM->SCGC4 |= SIM_SCGC4_UART2(1); //enables UART2 clock (60MHz)
    PORTE->PCR[16]=PORT_PCR_MUX(3);    //ties peripherals to mux address
    PORTE->PCR[17]=PORT_PCR_MUX(3);

    switch(rate){
    case(BIO_BIT_RATE_9600):
        UART2->BDH = 0x01U;
        UART2->BDL = 0x86U;
        UART2->C4 = 0x14U;
        break;
    case(BIO_BIT_RATE_19200):
        UART2->BDH = 0x00U;
        UART2->BDL = 0xC3U;
        UART2->C4 = 0x0AU;
        break;
    case(BIO_BIT_RATE_38400):
        UART2->BDH = 0x00U;
        UART2->BDL = 0x61U;
        UART2->C4 = 0x15U;
        break;
    case(BIO_BIT_RATE_57600):
        UART2->BDH = 0x00U;
        UART2->BDL = 0x41U;
        UART2->C4 = 0x04U;
        break;
    case(BIO_BIT_RATE_115200):
        UART2->BDH = 0x00U;
        UART2->BDL = 0x20U;
        UART2->C4 = 0x12U;
        break;
    default:    //Default to 9600bps
        UART2->BDH = 0x01U;
        UART2->BDL = 0x86U;
        UART2->C4 = 0x14U;
        break;
    }
    UART2->C2 |= UART_C2_TE_MASK;    //enables transmission
    UART2->C2 |= UART_C2_RE_MASK;    //enables receive

}

/*******************************************************************************************
* BIORead() - Checks for a character received
*    MCU: K65, UART2
*    return: ASCII character received or 0 if no character received
*******************************************************************************************/
INT8C BIORead(void){
    INT8C c;
    if ((UART2->S1 & UART_S1_RDRF_MASK) != 0){   //check if char received
        c = UART2->D;
    }else{
        c = '\0';                           //If not return 0
    }
    return (c);
}
/*******************************************************************************************
* BIOGetChar() - Blocks until character is received
*    return: INT8C ASCII character
*******************************************************************************************/
INT8C BIOGetChar(void){
    INT8C c;
    do{
        c = BIORead();
    }while(c == '\0');
    return c;
}

/*******************************************************************************************
* BIOWrite() - Sends an ASCII character
*              Blocks as much as one character time
*    MCU: K65, UART2
*    parameter: c is the ASCII character to be sent
*******************************************************************************************/
void BIOWrite(INT8C c){
    while ((UART2->S1 & UART_S1_TDRE_MASK)==0){} //waits until transmission
    UART2->D = (INT8U)c;                             //is ready
}

/*******************************************************************************************
* BIOPutStrg() - Writes a string to monitor
*    parameter: strg is a pointer to the ASCII string
*******************************************************************************************/
void BIOPutStrg(const INT8C *const strg){
    const INT8C *strgptr = strg;
    while (*strgptr != '\0'){              //until a null is reached
        BIOWrite(*strgptr);
        strgptr++;
    }
}

/*******************************************************************************************
* BIOOutDecByte() - Outputs the decimal value of a byte.
*    Parameters: bin is the byte to be sent,
*                lz is true if leading zeros are sent
*    Note: Deprecate. BIOOutDecWord() handles this
*******************************************************************************************/
void BIOOutDecByte (INT8U bin, INT8U lz){
    INT8C digits[3];
    INT8U lbin = bin;
    INT8U zon = lz;
    digits[0]=(INT8C)((lbin%10) + '0');
    lbin = lbin/10;
    digits[1]=(INT8C)((lbin%10)+'0');
    digits[2]=(INT8C)((lbin/10) +'0');

    if((digits[2] != '0') || (zon != 0)){
        BIOWrite(digits[2]);
        zon = TRUE;
    }else{
    }
    if((digits[1] != '0') || (zon != 0)){
        BIOWrite(digits[1]);
    }else{
    }
    BIOWrite(digits[0]);
}

/*******************************************************************************************
* BIOOutDecHWord() - Outputs a decimal value of two bytes.
*    Parameters: bin is the half word to be sent,
*                lz is true if leading zeros are sent
*    Note: Deprecate. BIOOutDecWord() handles this
*******************************************************************************************/
void BIOOutDecHWord (INT16U bin, INT8U lz){
    INT8C digits[5];
    INT16U lbin = bin;
    INT8U zon = lz;
    digits[0]=(INT8C)((lbin%10) +'0');
    lbin = lbin/10;
    digits[1]=(INT8C)((lbin%10)+'0');
    lbin = lbin/10;
    digits[2]=(INT8C)((lbin%10)+'0');
    lbin = lbin/10;
    digits[3]=(INT8C)((lbin%10)+'0');
    digits[4]=(INT8C)((lbin/10) +'0');

    if((digits[4] != '0') || (zon != 0)){
        BIOWrite(digits[4]);
        zon = TRUE;
    }else{
    }
    if((digits[3] != '0') || (zon != 0)){
        BIOWrite(digits[3]);
        zon = TRUE;
    }else{
    }
    if((digits[2] != '0') || (zon != 0)){
        BIOWrite(digits[2]);
        zon = TRUE;
    }else{
    }
    if((digits[1] != '0') || (zon != 0)){
        BIOWrite(digits[1]);
    }else{
    }
    BIOWrite(digits[0]);
    }

/*******************************************************************************************
* BIOOutDecWord() - Outputs a decimal value of four byte word.
*    Parameters: bin is the word to be sent,
*                maxlz is maximum number of leading zeros to be shown.
*    Examples:
*    One Byte = 10, maxlz = 1, Result: 10 - no leading zeros, at least one zero
*    One Byte = 10, maxlz = 3, Result: 010 - always three digits
*    One Half Word = 10, maxlz = 1, Result: 10 - no leading zeros, at least one zero
*    One Half Word = 10, maxlz = 5, Result: 00010 - always 5 digits
*******************************************************************************************/
void BIOOutDecWord (INT32U bin, INT8U maxlz){
    INT8C digits[10];
    INT32U lbin = bin;
    INT8U num_zeros = maxlz;
    INT8U dig_num;

    //Clamp leading zeros to acceptable values
    if(num_zeros > 10){
        num_zeros = 10;
    }else if(num_zeros < 1){
        num_zeros = 1;
    }else{
    }
    //Convert to ascii
    dig_num = 0;
    while(dig_num < 10){
        digits[dig_num] = (INT8C)((lbin % 10) +'0');
        lbin = lbin/10;
        dig_num++;
    }
    //Display ascii digits
    dig_num = 9;
    while(dig_num > 0){
        if((digits[dig_num] != '0') || (dig_num < num_zeros)){
            BIOWrite(digits[dig_num]);
            num_zeros = dig_num;            //show remaining zeros
        }else{
        }
        dig_num--;
    }
    BIOWrite(digits[0]);
    }

/*******************************************************************************************
* BIOGetStrg() - Inputs a string and stores it into an array.
*
* Descritpion: A routine that inputs a character string to an array until a carraige return
*              is received or strglen is exceeded.
*              Only printable characters are recognized except carriage return and backspace
*              Backspace erases displayed character and array character.
*              A NULL is always placed at the end of the string.
*              All printable characters are echoed.
* Return value: 0 -> if ended with CR
*               1 -> if strglen exceeded.
* Arguments: *strg is a pointer to the string array
*            strglen is the max string length, includes CR/NULL.
*******************************************************************************************/
INT8U BIOGetStrg(INT8U strglen,INT8C *const strg){
   INT8U charnum = 0;
   INT8C c;
   INT8C *strgp = strg;
   INT8U rvalue;
   c = BIOGetChar();
   while((c != '\r') && ((charnum <= (strglen-1)))){
       if((' ' <= c) && ('~' >= c) && (charnum < (strglen-1))){
           BIOWrite(c);
           *strgp = c;
           strgp++;
           charnum++;
           c=BIOGetChar();
       }else if(c == '\b'){
           BIOWrite('\b');
           BIOWrite(' ');
           BIOWrite('\b');
           strgp--;
           charnum--;
           c=BIOGetChar();
       }else if((' ' <= c) && ('~' >= c)){
           charnum++;
       }else{ /*non-printable character - ignore */
       }
   }
   BIOOutCRLF();
   *strgp = '\0';
   if(c == '\r'){
       rvalue = 0;
   }else{
       rvalue = 1;
   }
   return rvalue;
}

/*******************************************************************************************
* BIOOutCRLF() - Outputs a carriage return and line feed.
*
********************************************************************/
void BIOOutCRLF(void){
    BIOPutStrg("\r\n");
}

/*******************************************************************************************
* BIOHexStrgtoWord() - Converts a string of hex characters to a 32-bit
*                      word until NULL is reached.
* Return value: 0 -> if no error.
*               1 -> if string is too long for word.
*               2 -> if a non-hex character is in the string.
*               3 -> No characters in string. Started with NULL.
* Arguments: *strg is a pointer to the string array
*            *bin is the word that will hold the converted string.
*******************************************************************************************/
INT8U BIOHexStrgtoWord(INT8C *const strg,INT32U *bin){
    INT8U cnt = 0;
    INT32U lbin = 0;
    INT8C *strgptr = strg;
    INT8U rval = 0;
    if(*strgptr == '\0'){
        rval = 3;
    }else{
        while(*strgptr != '\0'){
            if(bioIsHex(*strgptr) != 0){
                lbin = (lbin << 4) | (INT32U)(bioHtoB(*strgptr));
            }else{
                rval = 2;
            }
            strgptr++;
            cnt++;
            if(cnt > 8){
                rval = 1;
            }else{
            }
        }
        *bin = lbin;
    }
    return rval;
}

/*******************************************************************************************
* BIOOutHexByte() - Output one byte in hex.
* bin is the byte to be sent
*******************************************************************************************/
void BIOOutHexByte(INT8U bin){
    BIOWrite(bioHtoA(bin>>4));
    BIOWrite(bioHtoA(bin & 0x0fu));
}

/*******************************************************************************************
* BIOOutHexHWord() - Output 16-bit word in hex.
* bin is the word to be sent
*******************************************************************************************/
void BIOOutHexHWord(INT16U bin){
    BIOOutHexByte((INT8U)(bin>>8));
    BIOOutHexByte((INT8U)(bin & 0x00ffu));
}
/*******************************************************************************************
* BIOOutHexWord() - Output 32-bit word in hex.
* bin is the word to be sent
* Todd Morton, 10/14/2014
*******************************************************************************************/
void BIOOutHexWord(INT32U bin){
    BIOOutHexByte((INT8U)(bin>>24));
    BIOOutHexByte((INT8U)(bin>>16));
    BIOOutHexByte((INT8U)(bin>>8));
    BIOOutHexByte((INT8U)(bin & 0x000000ff));
}
/*******************************************************************************************
* bioIsHex() - Checks for hex ascii character - private
* returns 1 if hex and 0 if not hex.
* Todd Morton, 10/14/2014
*******************************************************************************************/
static INT8U bioIsHex(INT8C c){
    INT8U rval;
    if((('0' <= c) && ('9' >= c)) || (('a' <= c) && ('f' >= c)) || (('A' <= c) && ('F' >= c))){
        rval = 1;
    }else{
        rval = 0;
    }
    return rval;
}

/*******************************************************************************************
* bioHtoB() - Converts a hex ascii character to a binary byte - private
* c is the ascii character to be converted.
* returns the binary value.
* Note: it returns a 0 if it is not a hex character - this should be fixed.
* Todd Morton, 10/14/2014
*******************************************************************************************/
static INT8U bioHtoB(INT8C c){
    INT8U bin;
    if(('0' <= c) && ('9' >= c)){
        bin = (INT8U)(c - '0');
    }else if(('a' <= c) && ('f' >= c)){
        bin = (INT8U)(c - 'a' + 0xa);
    }else if(('A' <= c) && ('F' >= c)){
        bin = (INT8U)(c - 'A' + 0xa);
    }else{
        bin = 0;
    }
    return bin;
}
/*******************************************************************************************
* bioHtoA() - Converts a hex nibble to ASCII - private
* hnib is the byte with the LSN to be sent
* Todd Morton, 10/14/2014
*******************************************************************************************/
static INT8C bioHtoA(INT8U hnib){
    INT8C asciic;
    INT8U hnmask = hnib & 0x0fu; /* Take care of any upper nibbles */
    if(hnmask <= 9U){
        asciic = (INT8C)(hnmask + 0x30U);
    }else{
        asciic = (INT8C)(hnmask + 0x37U);
    }
    return asciic;
}

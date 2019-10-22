#include <jorji.h>

#define BUFF_LEN 30 

#define ALARMLED  PIN_B2   //DORJI MODUL ile haberlesilemiyorsa bu led yanar ve yazilim durur. Hatayi tespit edip giderip, tekrar enerji verilmeli.
#define BUTON     PIN_B4   //BUTON yazdigimiz zaman aslinda PIN C3'u kastediyoruz demek.
#define LED       PIN_B5   //LED yazdigimiz zaman aslinda PIN C4u kastediyoruz.
#define SETA      PIN_C4   //Dorji nin SETA portuna bagli pinimizi kastediyoruz.
#define SETB      PIN_C5   //Dorji nin SETB portuna bagli pinimizi kastediyoruz.
                           //SETA ve SETB LOW ise TX-RX haberlesmesi. HIGH ise config ayarlama

#define ON      0          //OFF yazdigimiz zaman 0 anlasilsin.
#define OFF     1          //ON yazdigimiz zaman 1 anlasilsin.

char gelen[BUFF_LEN];      //char tipinde veri almak icin 30 karakterlik bir alan rezerve ediyoruz.
int1 mesaj_geldi=FALSE;    //mesaj geldiginde tum yazilim mesaj oldugunu algilasin.
char dorji_var = FALSE;    //Dorji modül ve cevap veriyorsa degeri TRUE olur.
int1 komut_modu = FALSE;   //Dorjiden gelen datalari dogru algilamak icin komut modu degiskeni kullandik. Normalde FALSE

int ctr=0;                 //gelen byte sayaci

//DORJI CONFIG - Bu bilgiler datasheet sayfa8'den bakilarak belirlenmeli. Simdilik default degerleri yazdik.
int32 drj_frq = 433920;    //dorji calisma frekansi. 433.92MHz default
int8  drj_mod = 3;         //dorji modulasyonu. 0,1,2,3,4,5 = 1,2,5,10,20,40 kbps (DRFSK)
int8  drj_pow = 7;         //dorji power. 0'dan 7ye kadar 3dbm artirir. 7 = 10dBm'dir.
int8  drj_rate = 3;        //dorji uart data rate. 0,1,2,3,4,5,6 = 1200, 2400, 4800, 9600, 19200, 38400, 57600
int8  drj_par = 0;         //dorji parity. 0,1,2 = No, Even, Odd
int8  drj_wakeup = 5;      //dorji wake up suresi. 1 saniye.

const char message[] = "BUTON BASILDI" ;
int8  dorji_cfg[7] = {0xFF, 0x56, 0xAE, 0x35, 0xA9, 0x55, 0xF0};  //Dorji'den config okuma komutu. Datasheet sayfa8.
int8  dorji_cfgw[7] = {0xFF, 0x56, 0xAE, 0x35, 0xA9, 0x55, 0x90}; //Droji config yazmada önce yollanacak kýsým.

//------------------Seri veri alma interrupti ------------------------------------------------- 
#INT_RDA 
void  RDA_isr(void) 
{ 
     
    int ch; 
    ch=getc(1);             //Seri kanaldan gelen 1 bytelik veriyi alalim.
    
    if (komut_modu == FALSE)
    {
       if (ch=='\n')          //gelen karakter LINEFEED degilse buffer'akaydedicez. LINEFEED ise, bufferdaki ilgili yeri 0 yapicaz ve mesaj geldi diycez
       { 
           gelen[ctr]='\0'; 
           ctr=0; 
           mesaj_geldi=TRUE; 
           return; 
       } 
           
       gelen[ctr++]=ch;       //geleni buffera yaziyyoruz
       if (ctr==BUFF_LEN) 
         ctr--;               //buffer tasarsa diye bir önlem
    }
    else
    {
         gelen[ctr++]=ch;     //gelen komut cevabýný buffera yaziyoruz.
         if (ctr==11)         //komuta tam olarakcevap gelmisse bitiriyoruz alimi.
         {
            komut_modu = FALSE;
            ctr=0; 
         }
    }
} 



void dorji_config()                 //Dorji modülü config moduna gecirir. Henuz gercekte bagli mi degil mi bilemiyoruz tabiki.
{
   output_high(SETA);               //Dorji modülü config moduna gecir.
   output_high(SETB);
   
   delay_ms(500);                   //Biraz bekle Dorji toparlansin.
}

void dorji_txrx()
{
   output_low(SETA);               //Dorji modülü tx-rx moduna gecir.
   output_low(SETB);
   
   delay_ms(500);                   //Biraz bekle Dorji toparlansin.

}

void dorji_kontrol()                //Dorji modül var mi yok mu ya da cevap veriyor mu bakilir.
{
   int8  txlen = 0;
   int8  rxlen = BUFF_LEN;
   int8  tout = 100;                //100 defa 10milisaniye bekleme.
   
   txlen = 7;     //dorji_cfg komutunun kac byte oldugu.
   
   komut_modu = TRUE;         //komut moduna gectigimizi bildirelim.
   while(rxlen)
   {
      gelen[rxlen--] = 0x00;  //gelen buffer alanini temizleyelim.
   }
   
   ctr = 0; //gelen byte buffer sayacini 0layalim.
   while(txlen)
   {
      fprintf(PORT1, "%i", dorji_cfg[--txlen]); //dorji komutunu gonder.
   }
   
   
   while((komut_modu == TRUE) && (tout))
   {
      delay_ms(10);  //cevap gelene kadar bekleyelim. komut cevabi 11 byte'tir ve geldiginde komut_modu false olur. o false olunca biz burdan otomatik cikariz.
      tout--;        //cevap sonsuza kadar gelmeyecek gibi ise 1 saniyeligine bekleyip cikalim.
   }
   
   if (tout)         //buraya timeouttan dolayi gelmissek Dorji yok. diger durumda dorji var. cunku cevap gelmis. tout 0 degilse dorjiden cevap gelmistir.
   {
      dorji_var = TRUE; 
      
      //drj_frq = (gelen[3] << 16) | (gelen[4] << 8) | gelen[5];  //mesela frekansi dorjiden okumak icin bunu ac
      
   }
   else
   {
      dorji_var = FALSE;
   }
   
   
}

void dorji_config_yaz()
{
   int8  txlen = 0;

   txlen = 7;     //dorji_cfg komutunun kac byte oldugu.

   if (dorji_var == FALSE)
   {
      output_low(ALARMLED);     //Dorji yok ya da haberlesilemiyor. Cevap gelmiyor.
      while(1);   //yazilim burada kalsin artik ilerlemesin.
   }

   while(txlen)
   {
      fprintf(PORT1, "%i", dorji_cfgw[--txlen]); //dorji komutunu gonder.
   }

   //geri kalan kismi gönderilecek. Yani asýl konfigürasyon
   
   fprintf(PORT1, "%i", (drj_frq >> 16)&0xff);  //frekans 24bit sayidir. yani 3 byte. once en ust anlamli bytei gonderecegiz.
   fprintf(PORT1, "%i", (drj_frq >> 8)&0xff);   //sonra 2.byte
   fprintf(PORT1, "%i", (drj_frq)&0xff);        //frekansin son byte. bakiniz datasheet sayfa 8
   
   fprintf(PORT1, "%i", drj_mod);               //DRFSK
   fprintf(PORT1, "%i", drj_pow);               //cikis gucu
   fprintf(PORT1, "%i", drj_rate);              //uart haberlesme hizi
   fprintf(PORT1, "%i", drj_par);               //parity
   fprintf(PORT1, "%i", drj_wakeup);            //kalkis suresi
   
   dorji_txrx();
   
   
}

void main()
{
   int buton_eski_durum = 0;     //Butonun eski durumunu ve yeni durumunu sakliycaz ki, butona ilk basiþta mesaj gönderelim. Yoksa basili tutuldugu surece yollariz.
   int buton_yeni_durum = 0;
   int led_durum = 0;            //Led durumunu, karsidaki JORJIN'den gelen buton bilgisine göre belirliycez. Karsidan her buton bilgisi geldiginde,
                                 //ledimiz sönükse yakicaz, yaniyorsa söndürücez.
   int8 i = 0, j;
                                 
                                 
   setup_adc(ADC_OFF); 
  
   delay_ms(500);                //Kalkista bekleyelim. Modüller voltaji alip kendine gelsin. 500 milisaniye yani yarým saniye.
   
   set_tris_b(0x10);
   output_high(ALARMLED);        //Ledi sondurelim.
   output_high(LED);             //Ledi sondurelim.

   enable_interrupts(INT_RDA);   //interruptlari acalim. seri kanaldan data gelirse isimiz bolunmeden interrupt alsin.
   enable_interrupts(GLOBAL); 
   
   dorji_config();               //Dorji modulu config moduna alalim. 
   dorji_kontrol();              //Dorji modul var mi yok mu bakalim.
   dorji_config_yaz();
   dorji_txrx();

   /*while(message[i] != '\0'){                     // mesaj yollama denemesi
    putc(message[i]);                
    delay_ms(1);                          
    i++;                                   
   }*/
  
   buton_eski_durum = TRUE;
   
   while(TRUE)                         //Bu döngünün icindekiler sonsuza kadar tekrarlansin.
   { 
       
         buton_yeni_durum = input(BUTON);   //buton okuyalim. Port C3'e bagli butonu okuyalim.
             
         if ((buton_yeni_durum == FALSE) && (buton_eski_durum == TRUE))     //Butona ilk basildigini anlamak icin boyle yaptik. Az once basili degildi. Simdi basildi.
         {
            buton_eski_durum = buton_yeni_durum;
            putc('B');                       //Butona basildigini JORJIN module bildirelim. O da diger JORJIN'e yollasin.
            delay_ms(1);
            putc('\n');
         }
         else if ((buton_yeni_durum == TRUE) && (buton_eski_durum == FALSE))
         {
            buton_eski_durum = TRUE;
         }
      
       if (mesaj_geldi)          //Karsidan veri gelmis. "gelen" degisken dizisinde gelenler yer aliyor.
       { 
           mesaj_geldi=FALSE; 
           if (gelen[0] == 'B')     //Karsidan "B" mesaji gelmisse, bunu ilk harfe bakarak anlayalim ve ledin durumunu verelim.
           {
               if (led_durum == 0)                          //Led sonukse yakalim, yaniyorsa sondurelim.
               {
                  led_durum = 1;
                  output_low(LED);                         //LED in portunu 1 yapalim
               }
               else 
               {
                  led_durum = 0;
                  output_high(LED);                        //LED in portunu 0 yapalim
               }
               
               gelen[0] = '0'; gelen[1] = '0';              //Gelen mesaj buffer'ini da temizleyelim ki bir dahaki sefer bizi yaniltmasin.
           }
       }          
   } 

}

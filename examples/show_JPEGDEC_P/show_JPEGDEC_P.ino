#define USE_TFT_LIB 0xA9341
#define USE_DMA

// JPEGDEC example for Adafruit GFX displays
#include <JPEGDEC.h>   //so we find "../test_images/tulips.h"
// Sample image (truncated) containing a 320x240 Exif thumbnail
#include "thumb_test.h"
#include "jpegs_lo.h"
#include "jpegs_lo2.h"
#include "bapa_80.h"
#include "betty.h"
#include "panda.h"
#include "../test_images/sciopero.h"
#include "../test_images/st_peters.h"
#include "../test_images/tulips.h"

#include "SPI.h"
#include "Adafruit_GFX.h"

#if 0
#elif defined(ARDUINO_RASPBERRY_PI_PICO)
#define TFT_CS  21
#define TFT_DC  19
#define TFT_RST 18
#define SD_CS   10
#elif defined(ESP32)
#define TFT_CS  5
#define TFT_DC  13
#define TFT_RST 12
#define SD_CS 17
#elif defined(ESP8266)
#define TFT_CS  D10
#define TFT_DC  D9
#define TFT_RST D8
#define SD_CS   D4
#else
#define TFT_CS  10
#define TFT_DC  9
#define TFT_RST 8
#define TFT_NO_CS 7
#define SD_CS   4
#endif

#if USE_TFT_LIB == 0x0000
#include <MCUFRIEND_kbv.h>
MCUFRIEND_kbv tft;
#define TFT_BEGIN() tft.begin(tft.readID())
#define TFT_STARTWRITE()
#define TFT_ENDWRITE()
#elif USE_TFT_LIB == 0xA7735
#include "Adafruit_ST7735.h"
Adafruit_ST7735 tft(TFT_CS, TFT_DC, TFT_RST);
#define TFT_BEGIN() tft.initR(INITR_BLACKTAB)
#elif USE_TFT_LIB == 0xA7796
#include "Adafruit_ST7796S_kbv.h"
Adafruit_ST7796S_kbv tft(TFT_CS, TFT_DC, TFT_RST);
#define TFT_BEGIN() tft.begin()
#elif USE_TFT_LIB == 0xA9341
#include "Adafruit_ILI9341.h"
Adafruit_ILI9341 tft(TFT_CS, TFT_DC, TFT_RST);
#define TFT_BEGIN() tft.begin()
#elif USE_TFT_LIB == 0xE8266   //Bodmer
#include <TFT_eSPI.h>
TFT_eSPI tft;
#if defined(USE_DMA)
#define TFT_BEGIN() {tft.begin(); tft.setSwapBytes(true); tft.initDMA(); }
#else
#define TFT_BEGIN() {tft.begin(); tft.setSwapBytes(true); }
#endif
#endif

#if !defined(TFT_STARTWRITE)
#define TFT_STARTWRITE() tft.startWrite()
#define TFT_ENDWRITE()   tft.endWrite()
#endif

JPEGDEC jpeg;
uint16_t esp_buf[480];

int JPEGDraw(JPEGDRAW *pDraw)
{
    int w = pDraw->iWidth;
    int h = pDraw->iHeight;
    int ht = jpeg.getHeight();
    int wid = jpeg.getWidth();
    while (ht > tft.height() || wid > tft.width()) ht /= 2, wid /= 2;  //was it scaled
    /*
        if (h != 8 && h != 16 && h != 4)
            Serial.printf("jpeg draw: @ (%d,%d) W x H = %dx%d [%dx%d]\n",
                          pDraw->x, pDraw->y, pDraw->iWidth, pDraw->iHeight, w, h);
    */
    //  Serial.printf("Pixel 0 = 0x%04x\n", pDraw->pPixels[0]);
#if USE_TFT_LIB == 0x0000
    tft.setAddrWindow(pDraw->x, pDraw->y, pDraw->x + w - 1, pDraw->y + h - 1);
    tft.pushColors(pDraw->pPixels, w * h, true);
#elif (USE_TFT_LIB & 0xF0000) == 0xA0000
    tft.dmaWait(); // Wait for prior writePixels() to finish
    tft.setAddrWindow(pDraw->x, pDraw->y, w, h);
#if defined(USE_DMA)
    tft.writePixels(pDraw->pPixels, w * h, false, false); //no block, big-endian
#else
    tft.writePixels(pDraw->pPixels, w * h, true, false); //default Use DMA, big-endian
#endif
#elif USE_TFT_LIB == 0xE8266
#if defined(USE_DMA)
    memcpy(esp_buf, pDraw->pPixels, w * h * 2);
    tft.pushImageDMA(pDraw->x, pDraw->y, w, h, esp_buf);
    //tft.pushImageDMA(pDraw->x, pDraw->y, w, h, pDraw->pPixels, esp_buf);
#else
    tft.pushImage(pDraw->x, pDraw->y, w, h, pDraw->pPixels);
#endif
#endif
    return 1;
} /* JPEGDraw() */

void setup() {
    Serial.begin(9600);
    //while (!Serial);
    Serial.println("Starting...");

    // put your setup code here, to run once:
    TFT_BEGIN();
    tft.setRotation(1); // PyPortal native orientation
} /* setup() */

void load_jpg_P(const uint8_t *arrayname, uint16_t array_size, const char* fn)
{
    long lTime;
    tft.fillScreen(0x001F); //TFT_BLUE
    if (jpeg.openFLASH((uint8_t *)arrayname, array_size, JPEGDraw))
    {
        lTime = micros();
        uint8_t scale = 0;
        int w = jpeg.getWidth(), h = jpeg.getHeight();
        uint8_t arg = 0;
        uint8_t rot = (w > h); //i.e. landscape
        if (tft.getRotation() != rot) tft.setRotation(rot);
        for (scale = 0; scale < 3; scale++) {
            if ((w >> scale) <= tft.width() && (h >> scale) <= tft.height()) break;
        }
        arg = (1 << scale); // | JPEG_AUTO_ROTATE;
        if (jpeg.hasThumb()) arg = JPEG_EXIF_THUMBNAIL;
        //jpeg.setMaxOutputSize(1);  //makes no difference
        TFT_STARTWRITE();
        if (jpeg.decode(0, 0, arg))
        {
            lTime = micros() - lTime;
            char buf[80];
            sprintf(buf, "%-16s %d x %d 1/%d, decode time = %d us\n",
                    fn, jpeg.getWidth(), jpeg.getHeight(), 1 << scale, (int)lTime);
            Serial.print(buf);
        }
        jpeg.close();
        TFT_ENDWRITE();
    }
    delay(10000); // pause between images
}

#define M8(x)    (x), sizeof(x), #x

void loop()
{
    load_jpg_P(M8(betty_8_jpg));
    load_jpg_P(M8(tiger_jpg));
    load_jpg_P(M8(angry_16_jpg));
    load_jpg_P(M8(bapa_80_jpg));
    //load_jpg_P(M8(purple_jpg));
    //load_jpg_P(M8(woof_jpg));
    load_jpg_P(M8(marilyn_95_jpg));
    load_jpg_P(M8(testcard_320x245x24_md_jpg));
    //load_jpg_P(M8(thumb_test));
    load_jpg_P(M8(sciopero));
    load_jpg_P(M8(st_peters));
    load_jpg_P(M8(tulips));
}

#if 0
void loop() {
    int i;
    long lTime;
    int iOption[4] = {0, JPEG_SCALE_HALF, JPEG_SCALE_QUARTER, JPEG_SCALE_EIGHTH};
    int iCenterX[4] = {0, 80, 120, 140};
    int iCenterY[4] = {0, 60, 90, 105};

    for (i = 0; i < 4; i++)
    {
        tft.fillScreen(ILI9341_BLACK);
        tft.startWrite(); // Not sharing TFT bus on PyPortal, just CS once and leave it
        //if (jpeg.openFLASH((uint8_t *)thumb_test, sizeof(thumb_test), JPEGDraw))
        //if (jpeg.openFLASH((uint8_t *)sciopero, sizeof(sciopero), JPEGDraw))
        if (jpeg.openFLASH((uint8_t *)tulips, sizeof(tulips), JPEGDraw))
            //if (jpeg.openFLASH((uint8_t *)tiger_jpg, sizeof(tiger_jpg), JPEGDraw))
        {
            lTime = micros();
            //if (jpeg.decode(iCenterX[i], iCenterY[i], JPEG_EXIF_THUMBNAIL | iOption[i]))
            if (jpeg.decode(iCenterX[i], iCenterY[i], iOption[i]))
            {
                lTime = micros() - lTime;
                Serial.printf("%d x %d image, decode time = %d us\n", jpeg.getWidth() >> i, jpeg.getHeight() >> i, (int)lTime);
            }
            jpeg.close();
        }
        delay(2000); // pause between images
        tft.endWrite(); // Not sharing TFT bus on PyPortal, just CS once and leave it
    } // for i
} /* loop() */
#endif //if 0

#if 0  //jpeg.c
for (y = 0; y < cy && bContinue; y++, jd.y += mcuCY)
{
    if (y == cy - 1) { // .kbv last tile row
        // e.g. 320x245 -> 160x123 i.e. 15 160x8 + 160x3
        int round = (1 << iScaleShift) - 1;
        int h = (pJPEG->iHeight + round) >> iScaleShift;
        if (h % jd.iHeight) jd.iHeight = h % jd.iHeight;
    }
#endif

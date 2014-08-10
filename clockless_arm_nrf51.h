#ifndef __INC_CLOCKLESS_ARM_NRF51
#define __INC_CLOCKLESS_ARM_NRF51

#if defined(NRF51)

#define TADJUST 0
#define TOTAL ( (T1+TADJUST) + (T2+TADJUST) + (T3+TADJUST) )
#define T1_MARK (TOTAL - (T1+TADJUST))
#define T2_MARK (T1_MARK - (T2+TADJUST))

#define SCALE(S,V) scale8_video(S,V)
// #define SCALE(S,V) scale8(S,V)

template <uint8_t DATA_PIN, int T1, int T2, int T3, EOrder RGB_ORDER = RGB, int XTRA0 = 0, bool FLIP = false, int WAIT_TIME = 500>
class ClocklessController : public CLEDController {
  typedef typename FastPinBB<DATA_PIN>::port_ptr_t data_ptr_t;
  typedef typename FastPinBB<DATA_PIN>::port_t data_t;

  data_t mPinMask;
  data_ptr_t mPort;
  CMinWait<WAIT_TIME> mWait;
public:
  virtual void init() {
    FastPinBB<DATA_PIN>::setOutput();
    mPinMask = FastPinBB<DATA_PIN>::mask();
    mPort = FastPinBB<DATA_PIN>::port();
  }

  virtual void clearLeds(int nLeds) {
    showColor(CRGB(0, 0, 0), nLeds, 0);
  }

  // set all the leds on the controller to a given color
  virtual void showColor(const struct CRGB & rgbdata, int nLeds, CRGB scale) {
    PixelController<RGB_ORDER> pixels(rgbdata, nLeds, scale, getDither());
    mWait.wait();
    cli();
    SysClockSaver savedClock(TOTAL);

    uint32_t clocks = showRGBInternal(pixels);

    // Adjust the timer
    long microsTaken = CLKS_TO_MICROS(clocks);
    long millisTaken = (microsTaken / 1000);
    savedClock.restore();
    // do { TimeTick_Increment(); } while(--millisTaken > 0);
    sei();
    mWait.mark();
  }

  virtual void show(const struct CRGB *rgbdata, int nLeds, CRGB scale) {
    PixelController<RGB_ORDER> pixels(rgbdata, nLeds, scale, getDither());
    mWait.wait();
    cli();
    SysClockSaver savedClock(TOTAL);

    // Serial.print("Scale is ");
    // Serial.print(scale.raw[0]); Serial.print(" ");
    // Serial.print(scale.raw[1]); Serial.print(" ");
    // Serial.print(scale.raw[2]); Serial.println(" ");
    // FastPinBB<DATA_PIN>::hi(); delay(1); FastPinBB<DATA_PIN>::lo();
    uint32_t clocks = showRGBInternal(pixels);

    // Adjust the timer
    long microsTaken = CLKS_TO_MICROS(clocks);
    long millisTaken = (microsTaken / 1000);
    savedClock.restore();
    // do { TimeTick_Increment(); } while(--millisTaken > 0);
    sei();
    mWait.mark();
  }

#ifdef SUPPORT_ARGB
  virtual void show(const struct CARGB *rgbdata, int nLeds, CRGB scale) {
    PixelController<RGB_ORDER> pixels(rgbdata, nLeds, scale, getDither());
    mWait.wait();
    cli();
    SysClockSaver savedClock(TOTAL);

    uint32_t clocks = showRGBInternal(pixels);

    // Adjust the timer
    long microsTaken = CLKS_TO_MICROS(clocks);
    long millisTaken = (microsTaken / 1000);
    savedClock.restore();
    // do { TimeTick_Increment(); } while(--millisTaken > 0);
    sei();
    mWait.mark();
  }
#endif

#if 0
// Get the arm defs, register/macro defs from the k20
#define ARM_DEMCR               *(volatile uint32_t *)0xE000EDFC // Debug Exception and Monitor Control
#define ARM_DEMCR_TRCENA                (1 << 24)        // Enable debugging & monitoring blocks
#define ARM_DWT_CTRL            *(volatile uint32_t *)0xE0001000 // DWT control register
#define ARM_DWT_CTRL_CYCCNTENA          (1 << 0)                // Enable cycle count
#define ARM_DWT_CYCCNT          *(volatile uint32_t *)0xE0001004 // Cycle count register

  template<int BITS> __attribute__ ((always_inline)) inline static void writeBits(register uint32_t & next_mark, register data_ptr_t port, register uint8_t & b)  {
    for(register uint32_t i = BITS; i > 0; i--) {
      while(ARM_DWT_CYCCNT < next_mark);
      next_mark = ARM_DWT_CYCCNT + (T1+T2+T3);
      *port = 1;
      uint32_t flip_mark = next_mark - ((b&0x80) ? (T3) : (T2+T3));
      b <<= 1;
      while(ARM_DWT_CYCCNT < flip_mark);
      *port = 0;
    }
  }

  // This method is made static to force making register Y available to use for data on AVR - if the method is non-static, then
  // gcc will use register Y for the this pointer.
  static void showRGBInternal(PixelController<RGB_ORDER> pixels) {
    register data_ptr_t port = FastPinBB<DATA_PIN>::port();
    *port = 0;

    // Setup the pixel controller and load/scale the first byte
    pixels.preStepFirstByteDithering();
    register uint8_t b = pixels.loadAndScale0();

      // Get access to the clock
    ARM_DEMCR    |= ARM_DEMCR_TRCENA;
    ARM_DWT_CTRL |= ARM_DWT_CTRL_CYCCNTENA;
    ARM_DWT_CYCCNT = 0;
    uint32_t next_mark = ARM_DWT_CYCCNT + (T1+T2+T3);

    while(pixels.has(1)) {
      pixels.stepDithering();

      // Write first byte, read next byte
      writeBits<8+XTRA0>(next_mark, port, b);
      b = pixels.loadAndScale1();

      // Write second byte, read 3rd byte
      writeBits<8+XTRA0>(next_mark, port, b);
      b = pixels.loadAndScale2();

      // Write third byte
      writeBits<8+XTRA0>(next_mark, port, b);
      b = pixels.advanceAndLoadAndScale0();
    };
  }
#else
// I hate using defines for these, should find a better representation at some point
#define _CTRL CTPTR[0]
#define _LOAD CTPTR[1]
#define _VAL CTPTR[2]
#define VAL (volatile uint32_t)(*((uint32_t*)(SysTick_BASE + 8)))

#define DT1(ADJ) delaycycles<T1-ADJ>();
#define DT2(ADJ) delaycycles<T2-ADJ>();
#define DT3(ADJ) delaycycles<T3-ADJ>();

#define HI2 FastPin<DATA_PIN>::hi();
#define LO2 FastPin<DATA_PIN>::lo();
#define BC2(BIT) if(b & 0x80) {} else { FastPin<DATA_PIN>::lo(); }
#define LSB1 b <<= 1;


#define FORCE_REFERENCE(var)  asm volatile( "" : : "r" (var) )
  // This method is made static to force making register Y available to use for data on AVR - if the method is non-static, then
  // gcc will use register Y for the this pointer.
  static uint32_t showRGBInternal(PixelController<RGB_ORDER> & pixels) {
    // Setup and start the clock
    // volatile uint32_t *CTPTR = &SysTick->CTRL;
    // _LOAD = ~(0xFF << 24);
    // _VAL = 0;
    // _CTRL |= SysTick_CTRL_CLKSOURCE_Msk;
    // _CTRL |= SysTick_CTRL_ENABLE_Msk;

    // register data_ptr_t port asm("r7") = 0; // FastPinBB<DATA_PIN>::port(); FORCE_REFERENCE(port);
    // *port = 0;

    // Setup the pixel controller and load/scale the first byte
    // pixels.preStepFirstByteDithering();
    register const uint8_t *pdata = pixels.mData;
    CRGB scale = pixels.mScale;


    register uint32_t b = scale8(pdata[RO(0)], scale.raw[RO(0)]);
    register uint32_t b2;
    int len = pixels.mLen;
    while(len >= 1) {
      HI2; DT1(4); BC2(7); DT2(2); LO2; LSB1; DT3(3);
      HI2; DT1(4); BC2(6); DT2(2); LO2; LSB1; DT3(3);
      HI2; DT1(4); BC2(5); DT2(2); LO2; LSB1; DT3(3);
      HI2; DT1(4); BC2(4); DT2(2); LO2; LSB1; DT3(3);
      HI2; DT1(4); BC2(3); DT2(2); LO2; LSB1; DT3(3);
      HI2; DT1(4); BC2(2); DT2(2); LO2; LSB1; DT3(3);
      HI2; DT1(4); BC2(1); DT2(2); LO2; LSB1; DT3(5); b2 = pdata[RO(1)];
      HI2; DT1(4); BC2(0); DT2(2); LO2; DT3(6);

      b = scale8(b2, scale.raw[RO(1)]);
      len--;

      HI2; DT1(4); BC2(7); DT2(2); LO2; LSB1; DT3(3);
      HI2; DT1(4); BC2(6); DT2(2); LO2; LSB1; DT3(3);
      HI2; DT1(4); BC2(5); DT2(2); LO2; LSB1; DT3(3);
      HI2; DT1(4); BC2(4); DT2(2); LO2; LSB1; DT3(3);
      HI2; DT1(4); BC2(3); DT2(2); LO2; LSB1; DT3(3);
      HI2; DT1(4); BC2(2); DT2(2); LO2; LSB1; DT3(3);
      HI2; DT1(4); BC2(1); DT2(2); LO2; LSB1; DT3(5); b2 = pdata[RO(2)];
      HI2; DT1(4); BC2(0); DT2(2); LO2; DT3(6);

      b = scale8(b2, scale.raw[RO(2)]);
      pdata += 3;

      HI2; DT1(4); BC2(7); DT2(2); LO2; LSB1; DT3(3);
      HI2; DT1(4); BC2(6); DT2(2); LO2; LSB1; DT3(3);
      HI2; DT1(4); BC2(5); DT2(2); LO2; LSB1; DT3(3);
      HI2; DT1(4); BC2(4); DT2(2); LO2; LSB1; DT3(3);
      HI2; DT1(4); BC2(3); DT2(2); LO2; LSB1; DT3(3);
      HI2; DT1(4); BC2(2); DT2(2); LO2; LSB1; DT3(6);
      HI2; DT1(4); BC2(1); DT2(2); LO2; LSB1; DT3(5); b2 = pdata[RO(0)];
      HI2; DT1(4); BC2(0); DT2(2); LO2; DT3(6);

      b = scale8(b2, scale.raw[RO(0)]);
    };

    return 0; // 0x00FFFFFF - _VAL;
  }
#endif
};


#endif // NRF51
#endif // __INC_CLOCKLESS_ARM_NRF51

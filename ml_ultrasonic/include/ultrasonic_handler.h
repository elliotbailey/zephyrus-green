#ifndef ML_ULTRASONIC
#define ML_ULTRASONIC

#ifdef __cplusplus
extern "C" {
#endif
    
    void ultrasonic_init(void);           
    float ultrasonic_read(void);
    void ultrasonic_publish(int category);

#ifdef __cplusplus
}
#endif

#endif
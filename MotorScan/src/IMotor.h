/**
 * ============================================================================
 * 檔案: IMotor.h
 * 描述: 馬達抽象介面
 * ============================================================================
 */
#ifndef IMOTOR_H
#define IMOTOR_H

class IMotor {
public:
    virtual ~IMotor() {}
    virtual void init() = 0;
    virtual bool update() = 0; 
    virtual float getCurrRPM() = 0;
    virtual void drive(int pwm) = 0;
    virtual void stop() = 0;

    virtual void print_details() {};
};

#endif
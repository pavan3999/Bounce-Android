#include <jni.h>
#include <android/log.h>
#include <algorithm>
#include <cstdint>
#include <vector>

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "BounceNative", __VA_ARGS__)

namespace {
constexpr int LOGICAL_W=128, LOGICAL_H=128, PLAYFIELD_H=96, OFFSCREEN_W=156, OFFSCREEN_H=96;

struct LevelRecord { uint8_t px,py,ox,oy; int8_t dx,dy; uint8_t wx,wy; };
struct Level {
    uint8_t s=0,S=0,format=0,W=0,V=0,ao=0,width=0,height=0;
    std::vector<uint8_t> tiles; std::vector<LevelRecord> records;
    int tileSize() const { return format ? 16 : 12; }
};

class Reader { const uint8_t* p; size_t n,i=0; public: Reader(const uint8_t*d,size_t n):p(d),n(n){} uint8_t u8(){return i<n?p[i++]:0;} };

Level parseLevel(const uint8_t* data,size_t size){
    Reader r(data,size); Level l;
    l.s=r.u8(); l.S=r.u8(); l.format=r.u8(); l.W=r.u8(); l.V=r.u8(); l.ao=r.u8();
    l.width=r.u8(); l.height=r.u8();
    l.tiles.resize((size_t)l.width*l.height); for(auto &v:l.tiles)v=r.u8();
    uint8_t count=r.u8(); l.records.resize(count);
    for(auto &o:l.records){o.px=r.u8();o.py=r.u8();o.ox=r.u8();o.oy=r.u8();o.dx=(int8_t)r.u8();o.dy=(int8_t)r.u8();o.wx=r.u8();o.wy=r.u8();}
    return l;
}

class Cheats {
    int state=0; bool advanced=false, invincible=false; int flyG=0;
public:
    void key(int k) {
        switch(k) {
        case '1': if(advanced) previous(); break;
        case '3': if(advanced) next(); break;
        case '5': if(advanced) invincible=true; break;
        case '#': if(advanced) flyG=300; break;
        case '7': if(state==0||state==2) ++state; else state=0; break;
        case '8': if(state==1||state==3) ++state; else if(state==5){invincible=true;state=0;} else state=0; break;
        case '9': if(state==4){advanced=true;state=0;} else state=0; break;
        default: state=0; break;
        }
    }
    void gameAction8(){ if(advanced) complete(); }
    bool isInvincible() const{return invincible;}
    int g() const{return flyG;}
private:
    void previous(){LOGI("cheat: previous level");}
    void next(){LOGI("cheat: next level");}
    void complete(){LOGI("cheat: complete level");}
};
Cheats cheats;
}

extern "C" JNIEXPORT void JNICALL Java_com_pavan3999_bounce_MainActivity_nativeInit(JNIEnv*,jobject){LOGI("Native Bounce initialized: %dx%d logical, %dx%d playfield",LOGICAL_W,LOGICAL_H,LOGICAL_W,PLAYFIELD_H);}
extern "C" JNIEXPORT void JNICALL Java_com_pavan3999_bounce_MainActivity_nativeKey(JNIEnv*,jobject,jint key){
    // Android keycode -> original Nokia numeric keys. ASCII handling is retained for the cheat layer.
    int k=0;
    switch(key){case 7:k='7';break;case 8:k='8';break;case 9:k='9';break;case 1:k='1';break;case 3:k='3';break;case 5:k='5';break;default:break;}
    if(k) cheats.key(k);
}
extern "C" JNIEXPORT void JNICALL Java_com_pavan3999_bounce_MainActivity_nativeTouch(JNIEnv*,jobject,jfloat,jfloat,jint){}

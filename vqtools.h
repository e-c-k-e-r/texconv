#pragma once

#include <vector>
#include <unordered_map>
#include <string>
#include <iostream>
#include <fstream>
#include <chrono>
#include <cmath>
#include <algorithm>

typedef uint32_t uint;

enum FilterMode {
    NEAREST, BILINEAR
};

struct RGBA {
    uint8_t r, g, b, a;
};

// N-dimensional vectors, for input to a VectorQuantizer.
template <uint N>
class Vec {
public:
    Vec(uint hval = 0) : hashVal(hval) {}
    Vec(const Vec<N>& other);
    void    zero();
    void    operator= (const Vec<N>& other);
    bool    operator== (const Vec<N>& other) const;
    void    operator+= (const Vec<N>& other);
    void    operator-= (const Vec<N>& other);
    Vec<N>  operator+ (const Vec<N>& other) const;
    Vec<N>  operator- (const Vec<N>& other) const;
    void    addMultiplied(const Vec<N>& other, float x);
    void    operator/= (float x);
    float&  operator[] (int index);
    const float&  operator[] (int index) const;
    void    set(int index, float value);
    float   lengthSquared() const;
    float   length() const;
    void    setLength(float len);
    void    normalize();
    void    print() const;
    static float distanceSquared(const Vec<N>& a, const Vec<N>& b);
    uint    hash() const;
    void    setHash(uint h) { hashVal = h; }
private:
    float   v[N];
    uint    hashVal; // Only used for the constant input vectors, so we only need to calc once.
    uint64_t lololol; // Speeds up the average compression by a couple of seconds on my machine. Probably some alignment stuff.
};

template<uint N>
inline Vec<N>::Vec(const Vec<N> &other) {
    *this = other;
}

template<uint N>
inline void Vec<N>::zero() {
    for (uint i=0; i<N; ++i)
        v[i] = 0;
}

template<uint N>
inline void Vec<N>::operator= (const Vec<N>& other) {
    for (uint i=0; i<N; ++i)
        v[i] = other.v[i];
    hashVal = other.hashVal;
}

template<uint N>
inline bool Vec<N>::operator== (const Vec<N>& other) const {
    for (uint i=0; i<N; ++i)
        if (fabs(v[i] - other.v[i]) > 0.001f)
            return false;
    return true;
}

template<uint N>
inline Vec<N> Vec<N>::operator+ (const Vec<N>& other) const {
    Vec<N> ret;
    for (uint i=0; i<N; ++i)
        ret.v[i] = v[i] + other.v[i];
    return ret;
}

template<uint N>
inline Vec<N> Vec<N>::operator- (const Vec<N>& other) const {
    Vec<N> ret;
    for (uint i=0; i<N; ++i)
        ret.v[i] = v[i] - other.v[i];
    return ret;
}

template<uint N>
inline void Vec<N>::addMultiplied(const Vec<N>& other, float x) {
    for (uint i=0; i<N; ++i)
        v[i] += (other.v[i] * x);
}

template<uint N>
inline void Vec<N>::operator+= (const Vec<N>& other) {
    for (uint i=0; i<N; ++i)
        v[i] += other.v[i];
}

template<uint N>
inline void Vec<N>::operator-= (const Vec<N>& other) {
    for (uint i=0; i<N; ++i)
        v[i] -= other.v[i];
}

template<uint N>
inline void Vec<N>::operator/= (float x) {
    const float invx = 1.0f / x;
    for (uint i=0; i<N; ++i)
        v[i] *= invx;
}

template<uint N>
inline float& Vec<N>::operator[] (int index) {
    return v[index];
}

template<uint N>
inline const float& Vec<N>::operator[] (int index) const {
    return v[index];
}

template<uint N>
inline void Vec<N>::set(int index, float value) {
    v[index] = value;
}

template<uint N>
inline float Vec<N>::length() const {
    return sqrt(lengthSquared());
}

template<uint N>
inline float Vec<N>::lengthSquared() const {
    float ret = 0;
    for (uint i=0; i<N; ++i)
        ret += (v[i] * v[i]);
    return ret;
}

template<uint N>
inline void Vec<N>::setLength(float len) {
    float x = (1.0f / length()) * len;
    for (uint i=0; i<N; ++i)
        v[i] *= x;
}

template<uint N>
inline void Vec<N>::normalize() {
    const float invlen = 1.0f / length();
    for (uint i=0; i<N; ++i)
        v[i] *= invlen;
}

template<uint N>
void Vec<N>::print() const {
    std::string str = "{ ";
    for (uint i=0; i<N; ++i) {
        str += std::to_string(v[i]);
        str += ' ';
    }
    str += '}';
    std::cout << str;
}

template<uint N>
float Vec<N>::distanceSquared(const Vec<N>& a, const Vec<N>& b) {
    return (a - b).lengthSquared();
}

template<uint N>
inline uint Vec<N>::hash() const {
    return hashVal;
}

template<uint N>
uint qHash(const Vec<N>& vec) {
    return vec.hash();
}

namespace std {
template<uint N>
struct hash<Vec<N>> {
    size_t operator()(const Vec<N>& v) const {
        uint32_t h=2166136261u;
        for(uint i=0;i<N;i++){
            uint32_t bits;
            memcpy(&bits,&v[i],sizeof(float));
            h ^= bits;
            h *= 16777619u;
        }
        return h ^ v.hash();
    }
};
}

template<uint N>
class VectorQuantizer {
public:
    struct Code {
        Vec<N> codeVec;
        Vec<N> vecSum;
        int vecCount = 0;
        float maxDistance = 0;
        Vec<N> maxDistanceVec;
    };

    std::vector<Code> codes;

    int codeCount() const { return (int)codes.size(); }
    const Vec<N>& codeVector(int i) const { return codes[i].codeVec; }

    int findClosest(const Vec<N>& vec) const;
    int findBestSplitCandidate() const;
    void removeUnusedCodes();
    void place(const std::unordered_map<Vec<N>,int>& vecs);
    void split();
    void splitCode(int index);
    void compress(const std::vector<Vec<N>>& vectors,int numCodes);
    bool writeReportToFile(const std::string& filename);
};

inline uint32_t packColor(const RGBA& c) {
    return (uint32_t(c.a)<<24)|(uint32_t(c.r)<<16)|(uint32_t(c.g)<<8)|uint32_t(c.b);
}
inline RGBA unpackColor(uint32_t argb) {
    RGBA c;
    c.a = (argb>>24)&0xFF;
    c.r = (argb>>16)&0xFF;
    c.g = (argb>>8)&0xFF;
    c.b = argb&0xFF;
    return c;
}

template<uint N>
inline void rgb2vec(uint32_t rgb, Vec<N>& vec, uint offset=0) {
    RGBA c = unpackColor(rgb);
    vec[offset+0] = c.r/255.f;
    vec[offset+1] = c.g/255.f;
    vec[offset+2] = c.b/255.f;
}

template<uint N>
inline void argb2vec(uint32_t argb, Vec<N>& vec, uint offset=0) {
    RGBA c = unpackColor(argb);
    vec[offset+0] = c.a/255.f;
    vec[offset+1] = c.r/255.f;
    vec[offset+2] = c.g/255.f;
    vec[offset+3] = c.b/255.f;
}

template<uint N>
inline void vec2rgb(const Vec<N>& vec, uint32_t& rgb, uint offset=0) {
    RGBA c;
    c.r = (uint8_t)(vec[offset+0]*255);
    c.g = (uint8_t)(vec[offset+1]*255);
    c.b = (uint8_t)(vec[offset+2]*255);
    c.a = 255;
    rgb = packColor(c);
}

template<uint N>
inline void vec2argb(const Vec<N>& vec, uint32_t& argb, uint offset=0) {
    RGBA c;
    c.a = (uint8_t)(vec[offset+0]*255);
    c.r = (uint8_t)(vec[offset+1]*255);
    c.g = (uint8_t)(vec[offset+2]*255);
    c.b = (uint8_t)(vec[offset+3]*255);
    argb = packColor(c);
}

template<uint N>
int VectorQuantizer<N>::findClosest(const Vec<N>& vec) const {
    if (codes.size() <= 1) return 0;
    int closestIndex = 0;
    float closestDist = Vec<N>::distanceSquared(codes[0].codeVec, vec);
    for(size_t i=1; i<codes.size(); i++) {
        float d = Vec<N>::distanceSquared(codes[i].codeVec, vec);
        if(d < closestDist){
            closestDist=d;
            closestIndex=(int)i;
            if (closestDist < 0.0001f) return closestIndex;
        }
    }
    return closestIndex;
}

template<uint N>
int VectorQuantizer<N>::findBestSplitCandidate() const {
    int idx=-1;
    float furthest=0;
    for(size_t i=0;i<codes.size();i++){
        if(codes[i].vecCount>1 && codes[i].maxDistance>furthest){
            furthest=codes[i].maxDistance;
            idx=(int)i;
        }
    }
    return idx;
}

template<uint N>
void VectorQuantizer<N>::removeUnusedCodes() {
    size_t oldSize=codes.size();
    codes.erase(
        std::remove_if(codes.begin(), codes.end(),
                       [](const Code& c){ return c.vecCount==0; }),
        codes.end()
    );
    if(codes.size()<oldSize){
        std::cout<<"Removed "<<(oldSize-codes.size())<<" unused codes\n";
    }
}

template<uint N>
void VectorQuantizer<N>::place(const std::unordered_map<Vec<N>,int>& vecs) {
    for(auto& code:codes){
        code.vecCount=0;
        code.vecSum.zero();
        code.maxDistance=0;
        code.maxDistanceVec.zero();
    }

    for(const auto& kv:vecs){
        const Vec<N>& vec=kv.first;
        int count=kv.second;
        Code& code = codes[findClosest(vec)];

        code.vecSum.addMultiplied(vec,count);
        code.vecCount+=count;

        float dist=Vec<N>::distanceSquared(code.codeVec,vec);
        if(dist>code.maxDistance){
            code.maxDistance=dist;
            code.maxDistanceVec=vec;
        }
    }

    for(auto& code:codes){
        if(code.vecCount>0){
            code.vecSum /= (float)code.vecCount;
            code.codeVec=code.vecSum;
        }
    }
}

template<uint N>
void VectorQuantizer<N>::split() {
    int SIZE=(int)codes.size();
    for(int i=0;i<SIZE;i++){
        if(codes[i].vecCount>1){
            splitCode(i);
        }
    }
}

template<uint N>
void VectorQuantizer<N>::splitCode(int index) {
    Code& code=codes[index];
    Vec<N> diff=code.maxDistanceVec-code.codeVec;
    diff.setLength(0.01f);
    Vec<N> newVec=code.codeVec;
    for(uint i=0;i<N;i++) newVec[i]+=diff[i];
    for(uint i=0;i<N;i++) code.codeVec[i]-=diff[i];
    Code newCode;
    newCode.codeVec=newVec;
    codes.push_back(newCode);
}

template<uint N>
void VectorQuantizer<N>::compress(const std::vector<Vec<N>>& vectors,int numCodes) {
    using clock=std::chrono::steady_clock;
    auto start=clock::now();

    std::unordered_map<Vec<N>,int> rle;
    for(const auto& v:vectors) rle[v]++;

    std::cout<<"RLE result: "<<vectors.size()<<" => "<<rle.size()<<"\n";

    codes.clear();
    codes.resize(1);
    codes.reserve(numCodes);
    place(rle);

    int splits=0, repairs=0;
    while((int)(codes.size()*2)<=numCodes){
        size_t before=codes.size();
        split();
        place(rle); place(rle); place(rle);
        removeUnusedCodes();

        if(codes.size()==before){
            std::cout<<"No further improvement by splitting\n";
            break;
        }
        splits++;
        std::cout<<"Split "<<splits<<" done. Codes: "<<codes.size()<<"\n";
    }

    while((int)codes.size()<numCodes){
        size_t before=codes.size();
        int n=numCodes-before;
        for(int i=0;i<n;i++){
            int idx=findBestSplitCandidate();
            if(idx==-1) break;
            splitCode(idx);
            codes[idx].maxDistance=0;
        }
        if(codes.size()==before){
            std::cout<<"No further improvement by repairing\n";
            break;
        }
        place(rle); place(rle); place(rle);
        removeUnusedCodes();
        repairs++;
        std::cout<<"Repair "<<repairs<<" done. Codes: "<<codes.size()<<"\n";
    }
    auto ms=std::chrono::duration_cast<std::chrono::milliseconds>(clock::now()-start).count();
    std::cout<<"Compression completed in "<<ms<<" ms\n";
}

template<uint N>
bool VectorQuantizer<N>::writeReportToFile(const std::string& fname){
    std::ofstream f(fname);
    if(!f.is_open()){
        std::cerr<<"Failed to open "<<fname<<"\n";
        return false;
    }
    for(int i=0;i<(int)codes.size();i++){
        f<<"Code: "<<i<<"\tUses: "<<codes[i].vecCount<<"\tError: "<<codes[i].maxDistance<<"\n";
    }
    return true;
}
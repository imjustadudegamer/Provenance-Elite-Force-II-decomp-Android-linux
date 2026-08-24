

#pragma once

#ifdef __cplusplus

class SkelVec4
{
public:
    union {
        float val[4];

        struct {
            float x;
            float y;
            float z;
            float w;
        } xyzw;
    };

protected:
    void copy(const SkelVec4&);

public:
    SkelVec4(float x, float y, float z, float w);
    SkelVec4(const SkelVec3& vec3, float w);
    SkelVec4(const float *vec4);

    operator float *();
    operator float *() const;

    void set(float x, float y, float z, float w);
};

#else

typedef struct {
    union {
        float val[4];

        struct {
            float x;
            float y;
            float z;
            float w;
        };
    };
} SkelVec4;

#endif

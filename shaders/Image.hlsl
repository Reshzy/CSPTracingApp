// Premultiplied BGRA textured quad. Overlay-local 2x3 affine and opacity live
// in the constant buffer; the source texture is not modified.
// pO = [[m00, m01, tx], [m10, m11, ty]] * [xR, yR, 1]; X-right, Y-down.
cbuffer Placement : register(b0)
{
    float2 overlaySize;
    float2 imageSize;
    float2 row0;
    float2 row1;
    float2 translation;
    float opacity;
    float _pad;
};

struct VSOut
{
    float4 pos : SV_Position;
    float2 uv : TEXCOORD0;
};

VSOut VSMain(uint id : SV_VertexID)
{
    float2 uv = float2(id & 1, id >> 1);
    float2 pR = uv * imageSize;
    float2 pixel = float2(
        row0.x * pR.x + row0.y * pR.y + translation.x,
        row1.x * pR.x + row1.y * pR.y + translation.y);
    float2 ndc = float2(-1.0, 1.0);
    if (overlaySize.x > 0.0 && overlaySize.y > 0.0)
    {
        ndc.x = pixel.x / overlaySize.x * 2.0 - 1.0;
        ndc.y = 1.0 - pixel.y / overlaySize.y * 2.0;
    }

    VSOut output;
    output.pos = float4(ndc, 0.0, 1.0);
    output.uv = uv;
    return output;
}

Texture2D imageTex : register(t0);
SamplerState imageSamp : register(s0);

float4 PSMain(VSOut input) : SV_Target
{
    // Source is already premultiplied (32bppPBGRA). Scale rgb and a together.
    float4 premul = imageTex.Sample(imageSamp, input.uv);
    premul *= opacity;
    return premul;
}

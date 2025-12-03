struct VSInput {
    float3 inPosition;
    float3 inColor;
    float2 inUV;
}

// see ResourceDescriptors.h for more
struct UniformBuffer {
    float4x4 model;
    float4x4 view;
    float4x4 proj;
};
ConstantBuffer<UniformBuffer> ubo;

struct VSOutput {
    float3 color;
    float4 pos : SV_Position;
    float2 fragUV;
};

[shader("vertex")]
// vertMain runs for every vertex
// calling our main function vertMain instead of main, this is because Slang and SPIR-V both support having multiple entry points in one file.
// SV_VertexID : Current vertex
//  Ususally piped to vertex buffer (VB)
//  In this case, static float2 positions
VSOutput vertMain(VSInput input) {
    VSOutput output;
    // dummy z and w components to produce clip coords (0.0 and 1.0 are dummy coords)
    // 'default' clip coords it appears
    // output.pos = float4(input.inPosition, 0.0, 1.0);
    
    output.pos = mul(ubo.proj, mul(ubo.view, mul(ubo.model, float4(input.inPosition, 1.0))));
    // match the color to the vertex
    output.color = input.inColor;
    output.fragUV = input.inUV;
    return output;
}

Sampler2D texture;

// produce a color and depth for the framebuffer (or framebuffers)
[shader("fragment")]
// The fragMain entry point function is called for every fragment
// pass the VSOutput to the fragment shader
float4 fragMain(VSOutput inVert) : SV_Target
{
    // rgba - [0, 1]
    //return float4(1.0, 0.0, 0.0, 1.0);
    // The color red is written to this outColor variable that is linked to the first (and only) framebuffer at index 0.
    
    //pull the color assigned at the vertex level
    //float3 color = inVert.color;
    // annnnnd, color based on vertex!
    // return float4(inVert.color, 1.0);
    //  the values for fragColor will be automatically interpolated for the fragments between the three vertices, resulting in a smooth gradient.
    return texture.Sample(inVert.fragUV);
}

// TODO: Research this: Another major feature of Slang is the ability to create shader libraries or modules;
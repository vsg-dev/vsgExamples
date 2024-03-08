void skipShadowData(inout int lightDataIndex, inout int shadowMapIndex)
{
    float shadowMapCount = lightData.values[lightDataIndex++].r;
    if (shadowMapCount > 0.0)
    {
        lightDataIndex += 4 * int(shadowMapCount);
        shadowMapIndex += int(shadowMapCount);
    }
}

float calculateShadowCoverageForDirectionalLight(inout int lightDataIndex, inout int shadowMapIndex, vec3 T, vec3 B, inout vec3 color)
{
    skipShadowData(lightDataIndex, shadowMapIndex);
    return 0;
}

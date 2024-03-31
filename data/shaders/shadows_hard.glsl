#ifdef VSG_SHADOWS_HARD
float calculateShadowCoverageForDirectionalLightHard(int lightDataIndex, int shadowMapIndex, vec3 T, vec3 B, inout vec3 color)
{
    vec4 shadowMapSettings = lightData.values[lightDataIndex++];
    int shadowMapCount = int(shadowMapSettings.r);

    bool matched = false;
    float overallCoverage = 0;
    while (shadowMapCount > 0 && !matched)
    {
        mat4 sm_matrix = mat4(lightData.values[lightDataIndex++],
                              lightData.values[lightDataIndex++],
                              lightData.values[lightDataIndex++],
                              lightData.values[lightDataIndex++]);

        vec4 sm_tc = sm_matrix * vec4(eyePos, 1.0);

        if (sm_tc.x >= 0.0 && sm_tc.x <= 1.0 && sm_tc.y >= 0.0 && sm_tc.y <= 1.0 && sm_tc.z >= 0.0 && sm_tc.z <= 1.0)
        {
            matched = true;
            overallCoverage = texture(sampler2DArrayShadow(shadowMaps, shadowMapShadowSampler), vec4(sm_tc.st, shadowMapIndex, sm_tc.z)).r;

#ifdef SHADOWMAP_DEBUG
            if (shadowMapIndex==0) color = vec3(1.0, 0.0, 0.0);
            else if (shadowMapIndex==1) color = vec3(0.0, 1.0, 0.0);
            else if (shadowMapIndex==2) color = vec3(0.0, 0.0, 1.0);
            else if (shadowMapIndex==3) color = vec3(1.0, 1.0, 0.0);
            else if (shadowMapIndex==4) color = vec3(0.0, 1.0, 1.0);
            else color = vec3(1.0, 1.0, 1.0);
#endif
        }

        lightDataIndex += 4;
        ++shadowMapIndex;
        --shadowMapCount;
    }


    if (shadowMapCount > 0)
    {
        // skip lightData and shadowMap entries for shadow maps that we haven't visited for this light
        // so subsequent light positions are correct.
        lightDataIndex += (8) * shadowMapCount;
        shadowMapIndex += shadowMapCount;
    }

    return overallCoverage;
}
#endif

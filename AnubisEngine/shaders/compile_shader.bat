@echo off
REM %1 = input <shader>.slang
REM %2 = target
REM %3 = profile
REM %4 = vert entry
REM %5 = frag entry
REM %6 = output name
C:/VulkanSDK/1.4.313.2/bin/slangc.exe %1 -target %2 -profile %3 -emit-spirv-directly -fvk-use-entrypoint-name -entry %4 -entry %5 -o %6
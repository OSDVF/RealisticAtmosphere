 # Realistic Atmosphere - Thesis implementation.
 # Copyright 2022 Ondøej Sabela. All rights reserved.
 # Uses ray tracing, path tracing and ray marching to create visually plausible outdoor scenes with atmosphere, terrain, clouds and analytical objects.

$FolderName = "shaders/glsl/"
if ( -not(Test-Path $FolderName) )
{
    New-Item $FolderName -ItemType Directory
}

Get-ChildItem -Recurse -filter "*.vert" | foreach{
   $name = $_.FullName | Resolve-Path -Relative
   .\bgfx\.build\win64_vs2017\bin\shadercRelease -f $name -o shaders/glsl/$_.bin --type v --platform windows --profile 440
 }

Get-ChildItem -Recurse -filter "*.frag" | foreach{
   $name = $_.FullName | Resolve-Path -Relative
   .\bgfx\.build\win64_vs2017\bin\shadercRelease -f $name -o shaders/glsl/$_.bin --type f --platform windows --profile 440
}

Get-ChildItem -Recurse -filter "*.comp" | foreach{
   $name = $_.FullName | Resolve-Path -Relative
   .\bgfx\.build\win64_vs2017\bin\shadercRelease -f $name -o shaders/glsl/$_.bin --type c --platform windows --profile 440
}
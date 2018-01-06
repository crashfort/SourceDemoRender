set Configuration=Release
msbuild "SampleExtension\SampleExtension.sln" /m:4 /t:Rebuild /clp:ErrorsOnly;WarningsOnly
msbuild "PreviewWindowExtension\PreviewWindowExtension.sln" /m:4 /t:Rebuild /clp:ErrorsOnly;WarningsOnly
msbuild "Direct2DContextExtension\Direct2DContextExtension.sln" /m:4 /t:Rebuild /clp:ErrorsOnly;WarningsOnly

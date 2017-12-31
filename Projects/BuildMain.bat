set Configuration=Release
msbuild "SourceDemoRender.sln" /m:4 /t:Rebuild /clp:ErrorsOnly;WarningsOnly

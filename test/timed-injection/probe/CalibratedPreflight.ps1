# Read-only planning; never launches a process or starts a study clock.
function Assert-PreflightDestinations {
 param([string]$ControlDir,[string[]]$Destinations)
 $root=[IO.Path]::GetFullPath($ControlDir.Replace('/','\')).TrimEnd('\')
 $seen=[Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
 foreach($destination in $Destinations){
  $path=[IO.Path]::GetFullPath($destination.Replace('/','\'))
  if(!$path.StartsWith($root+'\',[StringComparison]::OrdinalIgnoreCase)){throw "Outside control directory: $path"}
  if(!$seen.Add($path)){throw "Duplicate destination: $path"}
  if(Test-Path -LiteralPath $path){throw "Existing destination: $path"}
  $parent=[IO.Path]::GetDirectoryName($path)
  while($parent){
   if(Test-Path -LiteralPath $parent){
    $item=Get-Item -LiteralPath $parent -Force
    if(!$item.PSIsContainer -or ($item.Attributes -band [IO.FileAttributes]::ReparsePoint)){throw "Unsafe ancestor: $parent"}
   }
   $parent=[IO.Path]::GetDirectoryName($parent)
  }
 }
}
function New-CalibratedPreflightPlan {
 param([Parameter(Mandatory=$true)][string]$ControlDir,[Parameter(Mandatory=$true)][string]$Macro)
 $root=[IO.Path]::GetFullPath($ControlDir.Replace('/','\')).TrimEnd('\')
 $macroPath=(Resolve-Path -LiteralPath $Macro -ErrorAction Stop).Path
 $capture=Join-Path $root 'control-capture'
 $manifest=Join-Path $root 'control.manifest'
 $outputs=[Collections.Generic.List[string]]::new()
 foreach($suffix in '.packet','.energy','.chemistry'){$outputs.Add($capture+$suffix)}
 $outputs.Add($manifest)
 $jobs=@([pscustomobject]@{Name='control-capture';Stem=$capture;Executable='TrackCapture4um.exe';Arguments=@($macroPath,'capture',$capture,'900000001','900000002','900000003')})
 foreach($rate in 100,1000){
  $stem=Join-Path $root "control-$rate"
  $outputs.Add("$stem.history");$outputs.Add("$stem.schedule")
  $exposure=if($rate -eq 100){'20000000'}else{'2000000'}
  $endpoint=if($rate -eq 100){'20000002'}else{'2000002'}
  $jobs += [pscustomobject]@{Name="control-$rate";Stem=$stem;Executable='BeamOnHistory.exe';Arguments=@($macroPath,$manifest,"$stem.history",$exposure,$endpoint,'900000004','900000005','900000006','fixed',"$stem.schedule",'8')}
 }
 foreach($job in $jobs){
  foreach($suffix in '.log','.err','.meta','.modules','.args','.launcher-exit','.audit.log'){$outputs.Add($job.Stem+$suffix)}
 }
 Assert-PreflightDestinations $root $outputs.ToArray()
 if(Test-Path -LiteralPath $root){throw "Use a new control directory: $root"}
 [pscustomobject]@{ControlDir=$root;Manifest=$manifest;Destinations=$outputs.ToArray();Jobs=$jobs}
}

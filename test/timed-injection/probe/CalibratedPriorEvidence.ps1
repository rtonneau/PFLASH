function Get-CalibratedPriorEvidence {
 param([Parameter(Mandatory=$true)][string]$Root,[Parameter(Mandatory=$true)][string]$ActiveOutput)
 $active=[IO.Path]::GetFullPath($ActiveOutput).TrimEnd('\','/')
 # Runtime/build pruning applies to non-registry evidence only. Traverse these
 # trees for streams.tsv so synthetic and unexecuted assignments remain reserved.
 $pending=[Collections.Generic.Stack[object]]::new()
 $pending.Push(@([IO.Path]::GetFullPath($Root),$false))
 while($pending.Count){
  $entry=$pending.Pop(); $directory=[string]$entry[0]; $pruned=[bool]$entry[1]
  if($directory -eq $active -or $directory.StartsWith($active+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)){continue}
  $info=Get-Item -LiteralPath $directory -Force -ErrorAction Stop
  # Reject links instead of silently losing assignments or following cycles.
  if($info.Attributes -band [IO.FileAttributes]::ReparsePoint){throw "Reparse point in prior evidence: $directory"}
  $items=@(Get-ChildItem -LiteralPath $directory -Force -ErrorAction Stop)
  $pruned=$pruned -or $info.Name -in '.git','CMakeFiles','vcpkg_installed' -or @($items | Where-Object Name -eq 'CMakeCache.txt').Count -gt 0 -or (Test-Path -LiteralPath "$directory/lib/cmake/Geant4" -ErrorAction Stop) -or (Test-Path -LiteralPath "$directory/source/processes" -ErrorAction Stop)
  foreach($item in $items){
   if($item.PSIsContainer){$pending.Push(@($item.FullName,$pruned));continue}
   $selected=$item.Name -eq 'streams.tsv' -or (!$pruned -and ($item.Extension -in '.args','.history','.calibration' -or $item.Name -in 'histories.csv','runs.csv','results.csv' -or $item.Name -match '^(protocol-at-run|seed|capture-ledger)'))
   if($selected){
    if($item.Attributes -band [IO.FileAttributes]::ReparsePoint){throw "Reparse evidence file: $($item.FullName)"}
    $item
   }
  }
 }
}

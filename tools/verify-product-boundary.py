#!/usr/bin/env python3
"""Prevent retired product routes from reentering the supported build/UI."""
import argparse,hashlib,importlib.util,json,re,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def verify():
 features=json.loads((ROOT/'config/product-features.json').read_text())
 assert hashlib.sha256((ROOT/'apps/envoy/mainwindow.ui').read_bytes()).hexdigest()==features['protected_main_ui_sha256'],'Main window layout changed; review the baseline explicitly'
 project=(ROOT/'GetDAQ.pro').read_text();subdirs=re.search(r'^SUBDIRS\s*=\s*(.*)$',project,re.M).group(1).split()
 assert set(subdirs)=={'console','sentry','waveformviewer','cconfigeditor'},'Retired subproject reintroduced'
 for rel in ['apps/envoy/envoy.pro','apps/envoy/core.pri','apps/envoy/originalservices.pri','apps/deployment/nodeagent.pro']:
  p=ROOT/rel;text=p.read_text();assert not any(s in text for s in ['nativeruncontrol','eccsystemclient','hardwaresetupwindow','plantransitionwindow','scanplanwindow']),rel
  for token in text.split():
   if token.endswith(('.cpp','.h','.ui','.pri')) and '$$files' not in token:
    expanded=token.replace('$$PWD/',str(p.parent)+'/')
    if '$' not in expanded:assert (p.parent/expanded).exists(),f'Missing build input: {rel}: {token}'
 menus=(ROOT/'apps/envoy/mainwindow_menus.cpp').read_text()+(ROOT/'apps/envoy/mainwindow_recoverymenus.cpp').read_text()
 assert not any(s in menus for s in ['NativeColdHandoff','NativeOfflineScanPlans','NativePlanTransition','NativeTransferTasks','NativeCoBoMaintenance'])
 spec=importlib.util.spec_from_file_location('suite',ROOT/'tools/build-suite.py');suite=importlib.util.module_from_spec(spec);spec.loader.exec_module(suite)
 from types import SimpleNamespace
 args=SimpleNamespace(profile='full',root_config=[],cmake='cmake',qmake='qmake',make='make',jobs=2)
 steps=suite.plan(args,ROOT/'build-boundary-probe');programs={name for step in steps for name in step['programs']}
 assert {'GetDAQ','getEccSoapServer','DataExporter','getdaq-node-agent','getdaq-getcontroller','getdaq-sentry'}<=programs
 assert not programs & {'getdaq-ecc-server','getdaq-receiver','getdaq-supervisor','getdaq-control-mcp','getdaq-config','getdaq-config-check','getdaq-config-upload'}
 assert not any((ROOT/'apps'/name).exists() for name in ['ecc','datasupervisor','iceingress','configtool','configtransfer','configcheck'])
 print('PASS product baseline:',features['baseline'],'; main UI protected; original SOAP / receiver paths only')
if __name__=='__main__':verify()

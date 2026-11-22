"""Offline parity and CSV/API checks. Does not connect to the lamp or database."""
import csv, hashlib, io, json, os, tempfile, time
from pathlib import Path
import numpy as np
import pandas as pd
from siraj import Predictor, MODEL_ID
from server import application, csv_reading

ROOT=Path(__file__).resolve().parent.parent
test=pd.read_csv(ROOT/'verification/test_readings_165.csv')
expected=pd.read_csv(ROOT/'verification/expected_predictions.csv')
manifest=json.loads((ROOT/'model_manifest.json').read_text())
assert hashlib.sha256((ROOT/'backend/siraj_xgboost_model.json').read_bytes()).hexdigest()==manifest['model_sha256']
predictor=Predictor()
keys=['lux_ambient','human','vehicle','human_dist_cm','veh_dist_m','veh_speed_kmh','veh_approaching','lux_road','lamp_pct']
pred=[]; elapsed=[]
for _,row in test.iterrows():
    payload={k:float(row[k]) for k in keys}
    t=time.perf_counter(); result=predictor.predict(payload); elapsed.append(time.perf_counter()-t)
    pred.append(result['raw_pwm'])
np.testing.assert_allclose(pred,expected.cleaned_plus_synthetic,rtol=0,atol=0.00051)
sample={k:float(test.iloc[0][k]) for k in keys}
assert predictor.predict(dict(sample,lux_ambient=200))['pwm']==0
for flag in ['human','vehicle','veh_approaching']:
    try: predictor.predict(dict(sample,**{flag:2}))
    except ValueError: pass
    else: raise AssertionError('Invalid flag accepted')
try: predictor.predict(dict(sample,lux_road=float('nan')))
except ValueError: pass
else: raise AssertionError('NaN accepted')
def call(path,payload=None):
    body=json.dumps(payload).encode() if payload is not None else b''
    env={'PATH_INFO':path,'REQUEST_METHOD':'POST' if payload is not None else 'GET','CONTENT_TYPE':'application/json','CONTENT_LENGTH':str(len(body)),'wsgi.input':io.BytesIO(body)}
    statuses=[]
    response=b''.join(application(env,lambda status,headers:statuses.append(status)))
    return statuses[0],json.loads(response)
assert call('/health')[1]['model_id']==MODEL_ID
assert call('/predict',sample)[0]=='200 OK'
with tempfile.TemporaryDirectory() as temp:
    path=Path(temp)/'readings.csv'
    with path.open('w',newline='',encoding='utf-8-sig') as f:
        writer=csv.writer(f);writer.writerow(['lamp_id','seq']+keys);writer.writerow([1,123]+[sample[k] for k in keys])
    with path.open('rb') as f:
        f.readline();offset=f.tell();line=f.readline()
    os.environ['SIRAJ_READINGS_CSV']=str(path)
    ref=dict(offset=offset,lamp_id=1,seq=123,sha256=hashlib.sha256(line).hexdigest())
    assert csv_reading(ref)==sample
    status,result=call('/predict-csv',ref)
    assert status=='200 OK' and result==predictor.predict(sample)
    assert call('/predict-csv',dict(ref,sha256='bad'))[0]=='400 Bad Request'
    assert call('/predict-csv',dict(ref,seq=124))[0]=='400 Bad Request'
report={'status':'PASS','model_id':MODEL_ID,'historical_test_predictions_matched':165,'raw_rounding_tolerance':0.00051,'csv_hash_and_identity_checks':'PASS','daylight_and_input_validation':'PASS','max_local_prediction_ms':round(max(elapsed)*1000,3),'physical_device_test':'NOT RUN','php_runtime_test':'NOT RUN','firmware_compile':'NOT RUN'}
(ROOT/'verification/verification_result.json').write_text(json.dumps(report,indent=2))
print(json.dumps(report,indent=2))

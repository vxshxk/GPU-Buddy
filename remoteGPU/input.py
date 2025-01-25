!pip install groclake

import os
from groclake.cataloglake import CatalogLake

GROCLAKE_API_KEY = ''
GROCLAKE_ACCOUNT_ID = ''

os.environ['GROCLAKE_API_KEY'] = GROCLAKE_API_KEY
os.environ['GROCLAKE_ACCOUNT_ID'] = GROCLAKE_ACCOUNT_ID

catalog = CatalogLake()
from groclake.vectorlake import VectorLake
from groclake.datalake import DataLake
from groclake.modellake import ModelLake
try:
  vectorlake=VectorLake()
  vector_create=vectorlake.create()
  vectorlake_id=vector_create['vectorlake_id']
  print(vectorlake_id)
  datalake=DataLake()
  datalake_create=datalake.create()
  datalake_id=datalake_create['datalake_id']
  print(datalake_id)
except Exception as e:
  print(e)
try:
  payload_push={
      "datalake_id":datalake_id,
       "document_type": "url",
        "document_data": "https://drive.google.com/uc?id=1ak1uId4-1rqEIoiLZzraZ2KvKRX3kNwr&export=download"
  }
  data_push=datalake.push(payload_push)
  print(data_push)
  document_id=data_push.get("document_id")
  print(document_id)
except Exception as e:
  print(e)
try:
  payload_fetch = {
      "datalake_id": datalake_id,
      "document_id": document_id,
      "fetch_format" : "chunk",
      "chunk_size" : "500"
  }
  data_fetch = datalake.fetch(payload_fetch)
  document_chunks = data_fetch.get("document_data",[])
  print(document_chunks)
except Exception as e:
  print(e)
try:
  for idx,chunk in enumerate(document_chunks):
    print(chunk)
    vector_doc=vectorlake.generate(chunk)
    vector_chunk=vector_doc.get("vector")
    print(vector_chunk)
    vectorlake_push_payload={
        "vector" : vector_chunk,
        "vectorlake_id" : vectorlake_id,
        "document_text" : chunk,
        "vector_type" : "text",
        "metadata" : {}
    }
    push_response=vectorlake.push(vectorlake_push_payload)
    print(push_response)
except Exception as e:
  print(e)
try :
  search_query = "give answer for first quesetion"
  vector_search_data = vectorlake.generate(search_query)
  vector_search = vector_search_data.get("vector")
  print(vector_search)
  search_payload = {
      "vector" : vector_search,
      "vectorlake_id" : vectorlake_id,
      "vector_type" : "text"
  }
  search_response = vectorlake.search(search_payload)
  print(search_response)
except Exception as e:
  print(e)
import requests, json, base64

values = """
  {
    "gallery_name": "MyGallery"
  }
"""

headers = {
  'Content-Type': 'application/json',
  'app_id': '98a9ce6b',
  'app_key': '314e18bf9c959790db7be4e05e520b68'
}
request = requests.post('https://api.kairos.com/gallery/remove', data=values, headers=headers)

response_body = json.loads(request.text)
print(response_body)
import requests, json, base64

with open("Images/noam1.png", "rb") as imageFile:
    strr = base64.b64encode(imageFile.read())
    img = str(strr)
    img = img[2:len(img)-1]

values = """
  {
    "image": """;
values += '"'+(img)+'",';
values += """
    "gallery_name": "MyGallery"
  }
  """

headers = {
  'Content-Type': 'application/json',
  'app_id': '98a9ce6b',
  'app_key': '314e18bf9c959790db7be4e05e520b68'
}
request = requests.post('https://api.kairos.com/recognize', data=values, headers=headers)

response_body = json.loads(request.text)
if((str(response_body)).find("No match found") != -1):
    print("Not mached")
else:
    print("found it as ")
print(response_body)
import requests, json, base64
with open("Images/messi1.png", "rb") as imageFile:
    strr = base64.b64encode(imageFile.read())
    rd = str(imageFile)
    img = str(strr)
    img = img[2:len(img)-1]


values = """
  {
     "image": """;
values+= '"'+(img)+'",';
values += """
    "subject_id": "Messi Football player\n 2  childrens \n salary  1000  ",
    "gallery_name": "MyGallery"
  }
  """

headers = {
  'Content-Type': 'application/json',
  'app_id': '98a9ce6b',
  'app_key': '314e18bf9c959790db7be4e05e520b68'
}
request = requests.post('https://api.kairos.com/enroll', data=values, headers=headers)

response_body = json.loads(request.text)
print(response_body)

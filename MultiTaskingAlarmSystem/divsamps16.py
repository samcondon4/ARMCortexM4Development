a = [32768, 44544, 54656,61751,65024,64359,60313,53981,46753,40023,34919,
     32100,31667,33196,35885,38769,40960,41843,41200,36466,33614,31367,
     30223,30369,31642,33591,35599,37055,37511,36801,35078,32768,30458,
     28735,28025,28481,29937,31945,33894,35167,35313,34169,31922,29070,
     26306,24336,23693,24576,26767,29651,32340,33869,33436,30617,25513,
     18783,11555,5223,1177,512,3785,10880,20992,32768]

b = [x/16.0 for x in a] 

f = open('DacSampsdiv16.txt','a')
for k in b:
    i = int(k)
    f.write(str(i))
    f.write("u,")
    f.write("\n\r")


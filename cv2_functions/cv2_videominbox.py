import cv2
from numpy import *
v = cv2.VideoCapture(0)

while(1):
    ret,frame = v.read()
    frame = cv2.cvtColor(frame,cv2.COLOR_BGR2GRAY)
    s = cv2.Canny(frame,50,100)
    #a,s = cv2.threshold(frame,127,255,cv2.THRESH_BINARY)
    '''
    x = cv2.Sobel(frame,-1,1,0)
    y = cv2.Sobel(frame,-1,0,1)
    s = cv2.subtract(x,y)
    s = cv2.convertScaleAbs(s)
    a,s = cv2.threshold(s,20,255,cv2.THRESH_BINARY)    
    s = cv2.Laplacian(frame,-1,ksize=3)
    '''
    a,b,c = cv2.findContours(s,cv2.RETR_TREE,cv2.CHAIN_APPROX_SIMPLE)
    for i in b:
        x1,x2,x3,x4 = cv2.boxPoints(cv2.minAreaRect(i))
        x1 = tuple(x1)
        x2 = tuple(x2)
        x3 = tuple(x3)
        x4 = tuple(x4)
        cv2.line(frame,x1,x2,(0,255,0))
        cv2.line(frame,x2,x3,(0,255,0))
        cv2.line(frame,x3,x4,(0,255,0))
        cv2.line(frame,x4,x1,(0,255,0))
    cv2.imshow('frame',frame)
    cv2.imshow('frame2',s)
    if cv2.waitKey(42)==ord(' '):
        break

v.release()
cv2.destroyAllWindows()

import cv2

# Create a VideoCapture object to capture video from the webcam
cap = cv2.VideoCapture(0)

# Define the codec and create a VideoWriter object to save the video in MJPEG format
fourcc = cv2.VideoWriter_fourcc(*'MJPG')
out = cv2.VideoWriter('videos/mjpeg.mp4', fourcc, 20.0, (640, 480))

while cap.isOpened():
    ret, frame = cap.read()
    if not ret:
        break

    # Write the frame to the output file
    out.write(frame)

    cv2.imshow('video', frame)
    if cv2.waitKey(1) & 0xFF == ord('q'):
        break

# Release the VideoCapture and VideoWriter objects
cap.release()
out.release()

cv2.destroyAllWindows()


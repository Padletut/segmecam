import cv2
import numpy as np
import onnxruntime as ort
import torch
from torchvision import transforms
import mediapipe as mp

# Load ONNX model
model_path = 'models/model_v3_128x128.onnx'
session = ort.InferenceSession(model_path)

# Face detection
mp_face_detection = mp.solutions.face_detection
face_detection = mp_face_detection.FaceDetection(model_selection=0, min_detection_confidence=0.3)

# Webcam capture
cap = cv2.VideoCapture(0)
if not cap.isOpened():
    print("Cannot open webcam")
    exit()

# Preprocess: resize to 128x128, ToTensor
transform = transforms.Compose([
    transforms.ToPILImage(),
    transforms.Resize((128, 128)),
    transforms.ToTensor()
])

while True:
    ret, frame = cap.read()
    if not ret:
        break

    # Convert to RGB for MediaPipe
    rgb_frame = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
    results = face_detection.process(rgb_frame)

    overlay = frame.copy()  # Start with original

    if results.detections:
        for detection in results.detections:
            bboxC = detection.location_data.relative_bounding_box
            ih, iw, _ = frame.shape
            x, y, w, h = int(bboxC.xmin * iw), int(bboxC.ymin * ih), int(bboxC.width * iw), int(bboxC.height * ih)
            # Expand bbox slightly
            margin = 0.1
            x = max(0, int(x - margin * w))
            y = max(0, int(y - margin * h))
            w = min(iw - x, int(w * (1 + 2 * margin)))
            h = min(ih - y, int(h * (1 + 2 * margin)))

            face_crop = frame[y:y+h, x:x+w]
            if face_crop.size == 0:
                continue

            # Preprocess (convert BGR->RGB before Torch transforms)
            face_crop_rgb = cv2.cvtColor(face_crop, cv2.COLOR_BGR2RGB)
            input_tensor = transform(face_crop_rgb).unsqueeze(0).numpy().astype(np.float32)  # 1x3x128x128

            # Inference
            outputs = session.run(None, {'input': input_tensor})
            logits = outputs[0][0, 0]  # 128x128

            # Postprocess: sigmoid + threshold
            probs = 1 / (1 + np.exp(-logits))  # sigmoid
            mask = (probs > 0.5).astype(np.uint8) * 255  # binary mask

            # Resize mask to face crop size
            mask_resized = cv2.resize(mask, (w, h), interpolation=cv2.INTER_NEAREST)

            # Create colored mask (red)
            colored_mask = np.zeros_like(face_crop)
            colored_mask[:, :, 2] = mask_resized  # Red channel

            # Overlay on face crop
            face_overlay = cv2.addWeighted(face_crop, 0.7, colored_mask, 0.3, 0)

            # Put back on overlay
            overlay[y:y+h, x:x+w] = face_overlay

    # Display
    cv2.imshow('Wrinkle Segmentation', overlay)

    if cv2.waitKey(1) & 0xFF == ord('q'):
        break

cap.release()
cv2.destroyAllWindows()
face_detection.close()

School Bus Safety & Monitoring System
Πανεπιστήμιο Δυτικής Αττικής

Τμήμα: Μηχανικών Βιομηχανικής Σχεδίασης και Παραγωγής

Μάθημα: Κυβερνοφυσικά Συστήματα (2025)
Περιγραφή (Overview)
Το παρόν έργο αφορά την ανάπτυξη ενός ολοκληρωμένου Κυβερνοφυσικού Συστήματος (CPS) για την παρακολούθηση σχολικών λεωφορείων. 
Σκοπός είναι η ενίσχυση της ασφάλειας των μαθητών μέσω παρακολούθησης σε πραγματικό χρόνο, ανίχνευσης ατυχημάτων και ελέγχου της κατάστασης του οδηγού

Το σύστημα αποτελείται από τρία επίπεδα:

1. Vehicle Layer (Edge): Η συσκευή στο λεωφορείο που συλλέγει δεδομένα.
2. Server Layer: Ο κεντρικός διακομιστής που επεξεργάζεται τα δεδομένα μέσω MQTT.
3.Application Layer: Web Dashboard για τη διεύθυνση του σχολείου (Admin) και τους γονείς.

Δυνατότητες (Features)

Real-time GPS Tracking: Εντοπισμός θέσης και ταχύτητας οχήματος.
Ανίχνευση Ατυχήματος (Tilt Detection): Ανίχνευση ανατροπής ή επικίνδυνης κλίσης μέσω επιταχυνσιόμετρου.
Αλκοτέστ & Ανίχνευση Καπνού: Έλεγχος ατμόσφαιρας καμπίνας για αλκοόλ ή καπνό (αισθητήρας MQ-3).
Περιβαλλοντικές Συνθήκες: Καταγραφή θερμοκρασίας και υγρασίας εντός του οχήματος.
Live Video Stream: Μετάδοση εικόνας από την καμπίνα μέσω της κάμερας του ESP32.
Σύστημα Ειδοποιήσεων: Ηχητικές (Buzzer) και οπτικές (LEDs, LCD) ειδοποιήσεις, 
καθώς και αποστολή συμβάντων στο Dashboard.

Υλικό (Hardware)
Το σύστημα βασίζεται στον μικροελεγκτή FireBeetle 2 ESP32-S3.
Εξάρτημα	Περιγραφή
MCU	
DFRobot FireBeetle 2 ESP32-S3 (με κάμερα OV2640) 

GPS	
NEO-6M GPS Module 

Accelerometer	
Gravity LIS2DW12 (ή LIS331HH) 


Gas Sensor	
Waveshare MQ-3 (Alcohol/Ethanol) 

Env Sensor	
DHT11 (Temperature & Humidity) 

Display	
LCD 20x4 (I2C) 

Actuators	
Buzzer, LEDs (Red/Green), Push Button 

Power	
DFRobot AXP313A (Power Management)

Συνδεσμολογία (Pinout)
I2C (SDA)	Pin 1	Για Accelerometer & LCD
I2C (SCL)	Pin 2	Για Accelerometer & LCD
GPS RX	Pin 12	
GPS TX	Pin 13	
MQ-3 Sensor	Pin 10	Analog Input
DHT11	Pin 14	
Red LED	Pin 3	Ένδειξη Συναγερμού
Green LED	Pin 38	Ένδειξη Ομαλής Λειτουργίας
Buzzer	Pin 18	
Button	Pin 9	Ακύρωση Συναγερμού (Pull-up)

Λογισμικό & Βιβλιοθήκες (Software)
Ο κώδικας έχει γραφτεί σε C++ (Arduino Framework) και χρησιμοποιεί τις παρακάτω βιβλιοθήκες :


WiFi.h & PubSubClient.h (για συνδεσιμότητα & MQTT)

esp_camera.h (για το video streaming)

TinyGPSPlus.h (για ανάλυση δεδομένων NMEA GPS)

DFRobot_LIS2DW12.h (για το επιταχυνσιόμετρο)

DHT.h (για τον αισθητήρα θερμοκρασίας)

LiquidCrystal_I2C.h (για την οθόνη)

DFRobot_AXP313A.h (για διαχείριση ενέργειας)

Ρυθμίσεις (Configuration)

const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";
const char* MQTT_HOST = "YOUR_MQTT_BROKER_IP";
const uint16_t MQTT_PORT = 1884;
const char* BUS_ID = "bus01";

Εγκατάσταση & Λειτουργία
Συνδέστε τα εξαρτήματα σύμφωνα με το Pinout.
Εγκαταστήστε τις απαιτούμενες βιβλιοθήκες στο Arduino IDE.
Ενημερώστε τα WiFi και MQTT credentials στον κώδικα.
Κάντε Upload στο FireBeetle 2 ESP32-S3.
Εκκινήστε τον Local Host Server και το Web Dashboard.

Το σύστημα θα συνδεθεί αυτόματα στο WiFi και θα αρχίσει να στέλνει δεδομένα τηλεμετρίας (bus/bus01/telemetry) 
και συμβάντα (bus/bus01/event) στον MQTT broker.


Ομάδα Ανάπτυξης (Authors)

Μάρκος Βελαλόπουλος (ΑΜ: 21389027) 
Αλέξης Μπουστρής (ΑΜ: 21389171)

# **Background Research**

The following peer-reviewed sources directly support the design rationale, clinical need, and technical approach of our device:

## **Vibroacoustic therapy to treat pain in the temporomandibular joint**  
*Kitsantas et al. | ScienceDirect / Medical Hypotheses, 2024*
URL: https://www.sciencedirect.com/science/article/abs/pii/S0306987724000641  

**Relevance:**
- Establishes Vibroacoustic Therapy (VAT) as a safe, non-invasive, and plausible therapeutic approach for TMJ pain. 
- Confirms the clinical need and mechanism rationale for our therapy component. 
- Calls for trials — our device is a prototype toward that pipeline.

## **Local Vibratory Stimulation for Temporomandibular Disorder Myofascial Pain: A Randomised, Double-Blind, Placebo-Controlled Preliminary Study**  
*Calvo-Lobo et al. | PMC / BioMed Research International, 2020*
URL: https://pmc.ncbi.nlm.nih.gov/articles/PMC7735843/  

**Relevance:** 
- Randomized controlled trial (n=54) demonstrating statistically significant reduction in TMJ pain, muscular pain, and headache using local vibration therapy at 50-100 Hz applied to the masseter and temporal muscles.
- Directly validates our choice of vibration frequency and target anatomy.

## **sEMG and Vibration System Monitoring for Differential Diagnosis in Temporomandibular Joint Disorders**  
*Kulesa-Mrowiecka et al. | MDPI Sensors, 2022*
URL: https://www.mdpi.com/1424-8220/22/10/3811  

**Relevance:** 
- Demonstrates that vibration and sEMG signals can be used together for TMD differential diagnosis and rehabilitation monitoring.
- Supports our use of piezoelectric vibration sensing as a legitimate diagnostic modality for TMJ assessment.

## **Effects of vibratory feedback stimuli through an oral appliance on sleep bruxism: A 14-week intervention trial**
*Various | ScienceDirect / Journal of Prosthodontic Research, 2023*
URL: https://www.sciencedirect.com/science/article/pii/S1991790223003707

**Relevance:** 
- Directly validates the biofeedback-via-vibration approach for bruxism management over 14 weeks.
- A piezoelectric film sensor detects clenching events and triggers a vibration response — the same core loop as our device, which we have adapted into an external headband form factor

## **Sensor and Apparatus for Measurement of Muscle Activity in the Detection and Treatment of Bruxism Disorder**
*US Patent US10820853B2 | United States Patent Office, 2020*
URL: https://patents.google.com/patent/US10820853B2/en  

**Relevance:** 
- Validates the specific sensor placement approach we use: piezoelectric film on the temporalis muscle above the ear for bruxism detection, noting this location uniquely contracts during clenching but not during speech or normal facial movement — reducing false positives.

## **Toward Wearables for Bruxism Detection: Voluntary Oral Behaviors Sound Recorded Across the Head Depend on Transducer Placement**
*Nahhas et al. | PMC / Clinical and Experimental Dental Research, 2024*
URL: https://pmc.ncbi.nlm.nih.gov/articles/PMC11417139/

**Relevance:** 
- 2024 systematic analysis of wearable bruxism detection approaches.
- Explicitly identifies that current devices are limited by compliance issues with electrodes and intra-oral devices, and that there is a 'lack of real-world testing' — the exact gap our external wearable design targets.

**Research Summary:** Our device design is backed by: (1) a randomized controlled trial validating vibration therapy for TMD, (2) a 14-week clinical trial validating piezo-detected bruxism + vibration biofeedback, (3) a patent validating our exact sensor placement, (4) a systematic review identifying the gap we fill, and (5) engineering literature on piezoelectric sensors for jaw monitoring.

## **Design and Development of Wearable Vibration Assist Device for Tic Management**
*Pavithra et al. | IJRASET (Int'l Journal for Research in Applied Science & Engineering Technology), 2025*
URL: https://www.ijraset.com/research-paper/wearable-vibration-assist-device-for-tic-management  

**Relevance:**
- Uses ESP32 as core MCU with BLE and Arduino-based firmware, plus FreeRTOS task scheduling + MPU6050 IMU for motion detection at 50 Hz
- Kalman filter for noise reduction — same filtering approach applicable to PVDF ADC signal + Detection latency of ~200 ms from event to vibration feedback, with Bluetooth data logging (provides a validated latency benchmark for our system)
- ERM vibration motor driven via a BC547 NPN transistor switch (DIRECTLY shows transistor + PWM circuit we need to drive our motors from ESP32 GPIO
- Reference this source for block diagrams, transistor motor-drive circuit, FreeRTOS task structure, and BLE data logging

## **Development and in-vitro validation of an intraoral wearable biofeedback system for bruxism management**
*Al-Hamad et al. | Frontiers in Bioengineering and Biotechnology, 2025*
URL: https://doi.org/10.3389/fbioe.2025.1572970  

**Relevance:**
- Bite guard with integrated stress and vibration sensors + microcontroller and DAQ for bruxism detection.
- Force/piezo signals processed with AI algorithms to classify occlusal force levels.
- Shows how to establish sensing thresholds for jaw-force classification

## **Vibrotactile biofeedback devices in Parkinson's disease: a narrative review**
*Gonçalves et al. | Medical & Biological Engineering & Computing, 2021*
URL: https://link.springer.com/article/10.1007/s11517-021-02365-3

**Relevance:**
- Narrative review of wearable vibrotactile systems using ERM and LRA motors on belts and harnesses (surveys design patterns across dozens of published devices)
- Design rules: Actuator placement, motor count vs. cognitive load, vibration intensity and frequency ranges, and general biofeedback loop design guidelines
- Preference for miniaturized wearable actuators and sensors; biofeedback loop providing rescue strategies during or after event detection
- Vibrotactile cues proved suitable in any environment with positive patient adherence
- Reviews studies across vibration frequencies; findings are applicable to choosing our ERM motor operating frequency for temporalis placement

## **User Participatory Design of a Wearable Focal Vibration Device for HomeBased Stroke Rehabilitation**
*Wang et al. | Sensors, MDPI, 2022*
URL: https://www.mdpi.com/1424-8220/22/9/3308/review\_report
**Relevance:**
- References for hardware design: Motors draw ~80 mA (130 mA peak), confirms that we need a transistor or MOSFET driver, not direct GPIO drive + 3D-printed PLA pods sealed with epoxy
- PWM intensity control compensates for battery voltage drop over discharge
- Single-cell 500 mAh LiPo with TP4056-style charging circuit and battery voltage monitoring via microcontroller

## **Soft skin-interfaced mechano-acoustic sensors and haptic actuators for wireless monitoring and treatment of TMJ disorders**
*Kang et al. | npj Digital Medicine (Nature Publishing Group), 2022*
URL: https://www.nature.com/articles/s41746-022-00691-w  

**Relevance:**
- Dual IMUs + mechano-acoustic sensing for jaw monitoring; BLE streaming to GUI with real-time analytics -> demonstrates clinical feasibility of external wearable jaw sensing
- Flexible PCB with LiPo battery in a skin-conformal patch design provides reference for conformal wearable packaging on the head/jaw region

## **A self-powered flexible piezoelectric sensor patch for deep learning-assisted motion identification and rehabilitation training system**
*Guo et al. | Nano Energy (Elsevier), 2024*
URL: https://doi.org/10.1016/j.nanoen.2024.109427  

**Relevance:**
- Uses PVDF fibrous film as a flexible piezoelectric sensor patch for joint motion monitoring and provides signal characteristics (voltage output, sensitivity, frequency response) <- directly applicable to our PVDF sensor on the temporalis
- Signal processing path: Piezoelectric signal -> feature extraction -> LSTM deep learning classifier for motion identification
- Quantitative sensitivity and noise floor data for PVDF on joints (useful for setting our ADC threshold and understanding what signal amplitude to expect from the temporalis muscle)

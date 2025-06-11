// app/index.tsx
import React, { useState, useEffect } from 'react';
import {
  SafeAreaView,
  View,
  Text,
  StyleSheet,
  ScrollView,
  TouchableOpacity,
  Linking,
} from 'react-native';
import Slider from '@react-native-community/slider';
import Annuli from '../annuli'; // import your Annuli component

// Define your modes with descriptive labels.
const modes = [
  { id: 0, label: 'Rainbow Cycle' },
  { id: 1, label: 'Solid Blue' },
  { id: 2, label: 'Flash White' },
  { id: 3, label: 'Theatre Chase' },
  { id: 4, label: 'Side Wipe' },
  { id: 5, label: 'Sparkle' },
  { id: 6, label: 'Sinelon' },
  { id: 7, label: 'Juggle' },
  { id: 8, label: 'Running Lights' },
  { id: 9, label: 'Laser Sweep' },
  { id: 10, label: 'Strobe Fade' },
  { id: 11, label: 'Orbiting Comets' },
  { id: 12, label: 'Color Bounce' },
  { id: 13, label: 'Psychedelic Swirl' },
  { id: 14, label: 'Neon Grid' },
  { id: 15, label: 'Echo Waves' },
  { id: 16, label: 'Firefly Dance' },
  { id: 17, label: 'Spinning Bar' },
  { id: 18, label: 'Liquid Ripple' },
  { id: 19, label: 'Full Throttle' },
  { id: 20, label: 'Rave Strobe' },
  { id: 21, label: 'Thunder Pulse' },
  { id: 22, label: 'Shockwave' },
  { id: 23, label: 'Bass Drop' },
  { id: 24, label: 'Dynamic Bar' },
  { id: 25, label: 'Sonic Slicer' },
  { id: 26, label: 'Radial Surge' },
];

export default function HomeScreen() {
  const [selectedMode, setSelectedMode] = useState(0);
  const [brightnessPercent, setBrightnessPercent] = useState(50);
  const [isConnected, setIsConnected] = useState(false);

  // For AP mode, the ESP32 usually has a fixed IP (adjust if necessary)
  const ESP32_IP = '192.168.4.1';

  // Function to send a command to the ESP32
  const sendCommand = (parameter: string, value: number) => {
    const url = `http://${ESP32_IP}/set?${parameter}=${value}`;
    fetch(url)
      .then(response => response.text())
      .then(text => console.log(`Sent ${parameter}=${value}:`, text))
      .catch(error => console.error('Error:', error));
  };

  const handleModeChange = (mode: number) => {
    setSelectedMode(mode);
    sendCommand('mode', mode);
  };

  const handleBrightnessChange = (value: number) => {
    setBrightnessPercent(value);
    const brightnessValue = Math.round((value / 100) * 255);
    sendCommand('brightness', brightnessValue);
    console.log('Sliding complete. Brightness set to:', brightnessValue);
  };

  // Connectivity check: try to fetch /status from the ESP32.
  const checkConnection = async () => {
    try {
      const response = await fetch(`http://${ESP32_IP}/status`);
      if (response.ok) {
        setIsConnected(true);
      } else {
        setIsConnected(false);
      }
    } catch (error) {
      setIsConnected(false);
    }
  };

  useEffect(() => {
    checkConnection();
    const interval = setInterval(checkConnection, 5000);
    return () => clearInterval(interval);
  }, []);

  // Optional: Open device WiFi settings to help the user connect.
  const openWiFiSettings = () => {
    Linking.openURL('app-settings:');
  };

  return (
    <SafeAreaView style={styles.safeContainer}>
      <ScrollView contentContainerStyle={styles.container}>
        <Text style={styles.title}>SmartGlasses Control</Text>

        {/* Render Annuli only if connected */}
        {true ? (
          <View style={styles.annuliContainer}>
            <Annuli />
          </View>
        ) : (
          <View style={styles.connectContainer}>
            <Text style={styles.connectText}>Not connected to SmartGlasses.</Text>
            <TouchableOpacity onPress={openWiFiSettings} style={styles.connectButton}>
              <Text style={styles.connectButtonText}>Connect Now</Text>
            </TouchableOpacity>
          </View>
        )}

        {/* Brightness Controls (positioned just below Annuli) */}
        <Text style={styles.subtitle}>Brightness: {brightnessPercent}%</Text>
        <Slider
          style={styles.slider}
          minimumValue={0}
          maximumValue={100}
          step={1}
          value={brightnessPercent}
          onValueChange={(value) => setBrightnessPercent(value)}
          onSlidingComplete={handleBrightnessChange}
          minimumTrackTintColor="#007AFF"
          maximumTrackTintColor="#ccc"
          thumbTintColor="#007AFF"
        />

        {/* Mode Selection */}
        <Text style={styles.subtitle}>Select Mode</Text>
        <View style={styles.modeGrid}>
          {modes.map((mode) => (
            <TouchableOpacity
              key={mode.id.toString()}
              style={[
                styles.modeItem,
                selectedMode === mode.id && styles.selectedModeItem,
              ]}
              onPress={() => handleModeChange(mode.id)}
            >
              <Text style={styles.modeText}>{mode.label}</Text>
            </TouchableOpacity>
          ))}
        </View>
      </ScrollView>
    </SafeAreaView>
  );
}

const styles = StyleSheet.create({
  safeContainer: {
    flex: 1,
    backgroundColor: '#000',
  },
  container: {
    alignItems: 'center',
    paddingVertical: 20,
    paddingHorizontal: 20,
  },
  title: {
    color: '#fff',
    fontSize: 28,
    fontWeight: 'bold',
    marginTop: 40, // extra top margin to clear dynamic island
    marginBottom: 10,
    textAlign: 'center',
  },
  annuliContainer: {
    width: '100%',
    height: 300, // Fixed height for Annuli animation; adjust as needed.
    alignItems: 'center',
    justifyContent: 'center',
    marginBottom: 20,
  },
  connectContainer: {
    width: '100%',
    height: 300,
    alignItems: 'center',
    justifyContent: 'center',
    marginBottom: 20,
    backgroundColor: '#111',
  },
  connectText: {
    color: '#fff',
    fontSize: 18,
    marginBottom: 10,
  },
  connectButton: {
    backgroundColor: '#007AFF',
    paddingVertical: 10,
    paddingHorizontal: 20,
    borderRadius: 8,
  },
  connectButtonText: {
    color: '#fff',
    fontSize: 16,
  },
  subtitle: {
    color: '#fff',
    fontSize: 20,
    marginVertical: 10,
    textAlign: 'center',
  },
  slider: {
    width: '90%',
    height: 40,
    marginTop: 20,
    marginBottom: 20,
  },
  modeGrid: {
    flexDirection: 'row',
    flexWrap: 'wrap',
    justifyContent: 'center',
  },
  modeItem: {
    backgroundColor: '#333',
    paddingVertical: 10,
    paddingHorizontal: 12,
    margin: 6,
    borderRadius: 8,
    minWidth: 100,
    alignItems: 'center',
  },
  selectedModeItem: {
    backgroundColor: '#007AFF',
  },
  modeText: {
    color: '#fff',
    fontSize: 14,
    textAlign: 'center',
  },
});


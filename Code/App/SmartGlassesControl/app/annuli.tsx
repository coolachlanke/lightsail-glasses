// app/annuli.tsx
import React, { useState, useEffect, useMemo } from 'react';
import { Dimensions } from 'react-native';
import Svg, { Circle, G } from 'react-native-svg';

const { width: viewportWidth } = Dimensions.get('window');
// 2% padding on each side.
const padding = viewportWidth * 0.02;
const effectiveWidth = viewportWidth - 2 * padding;
const canvasSize = viewportWidth; // Use full viewport width for the SVG.

// Annulus settings:
const outerRadius = effectiveWidth / 4;           // Outer radius.
const ringThickness = 30;                         // Ring thickness.
const innerRadius = outerRadius - ringThickness;    // Inner radius.
const strokeWidth = 2;                            // Outline thickness (if used).

// Annuli centers:
const leftCenterX = padding + effectiveWidth / 4;
const rightCenterX = padding + (3 * effectiveWidth) / 4;
const centerY = padding + effectiveWidth / 2;

// LED (glow) settings:
const numLEDs = 16;    // Number of LED glow groups per annulus.
const ledAngles = Array.from({ length: numLEDs }, (_, i) => i * (360 / numLEDs));
// LED centers will be placed on a circle halfway between outer and inner radii.
const desiredRadius = (outerRadius + innerRadius) / 2;

// Glow settings:
const baseGlowRadius = 10;    // Base glow circle radius.
const numGlowLayers = 5;      // Number of glow circles per LED.
const deltaGlow = 2;          // Additional radius per glow layer.
const baseGlowOpacity = 0.8;  // Opacity of the innermost glow.
const sigma = 4;              // Standard deviation for Gaussian falloff.

// Helper: Convert HSV (h in degrees, s and v in [0,1]) to hex color.
function hsvToHex(h: number, s: number, v: number) {
  const f = (n: number, k = (n + h / 60) % 6) =>
    v - v * s * Math.max(Math.min(k, 4 - k, 1), 0);
  const r = Math.round(f(5) * 255);
  const g = Math.round(f(3) * 255);
  const b = Math.round(f(1) * 255);
  return '#' + ((1 << 24) + (r << 16) + (g << 8) + b).toString(16).slice(1);
}

// Memoize LED positions for a given annulus center.
const useLEDPositions = (annulusCenterX: number, annulusCenterY: number) =>
  useMemo(() => {
    return ledAngles.map(angle => {
      const angleRad = (angle * Math.PI) / 180;
      const x_center = annulusCenterX + desiredRadius * Math.cos(angleRad);
      const y_center = annulusCenterY + desiredRadius * Math.sin(angleRad);
      return {
        angle,
        x_center,
        y_center,
        rotation: angle - 90,
      };
    });
  }, [annulusCenterX, annulusCenterY]);

export default function Annuli() {
  // Global hue that evolves over time.
  const [gHue, setGHue] = useState(0);

  // Update global hue every 50ms.
  useEffect(() => {
    const interval = setInterval(() => {
      setGHue(prev => (prev + 5) % 360);
    }, 50);
    return () => clearInterval(interval);
  }, []);

  const leftLEDData = useLEDPositions(leftCenterX, centerY);
  const rightLEDData = useLEDPositions(rightCenterX, centerY);

  return (
    <Svg height={canvasSize} width={canvasSize}>
      {/* (Optional) You could render annuli circles here if needed */}

      {/* Render LED glow groups for left annulus */}
      {leftLEDData.map((led, index) => {
        const effectiveHue = (led.angle + gHue) % 360;
        const fillColor = hsvToHex(effectiveHue, 1, 1);
        return (
          <G key={`left-led-${index}`}>
            {Array.from({ length: numGlowLayers }, (_, i) => {
              const rGlow = baseGlowRadius + i * deltaGlow;
              const opacity = baseGlowOpacity * Math.exp(-((i * deltaGlow) ** 2) / (2 * sigma * sigma));
              return (
                <Circle
                  key={`left-glow-${index}-${i}`}
                  cx={led.x_center}
                  cy={led.y_center}
                  r={rGlow}
                  fill={fillColor}
                  opacity={opacity}
                />
              );
            })}
          </G>
        );
      })}

      {/* Render LED glow groups for right annulus */}
      {rightLEDData.map((led, index) => {
        const effectiveHue = (led.angle + gHue) % 360;
        const fillColor = hsvToHex(effectiveHue, 1, 1);
        return (
          <G key={`right-led-${index}`}>
            {Array.from({ length: numGlowLayers }, (_, i) => {
              const rGlow = baseGlowRadius + i * deltaGlow;
              const opacity = baseGlowOpacity * Math.exp(-((i * deltaGlow) ** 2) / (2 * sigma * sigma));
              return (
                <Circle
                  key={`right-glow-${index}-${i}`}
                  cx={led.x_center}
                  cy={led.y_center}
                  r={rGlow}
                  fill={fillColor}
                  opacity={opacity}
                />
              );
            })}
          </G>
        );
      })}
    </Svg>
  );
}

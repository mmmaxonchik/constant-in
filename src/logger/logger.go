package logger

import (
	"fmt"
	"log"
	"os"
	"strings"
)

type Logger interface {
	Errorf(format string, args ...any)
	Error(args ...any)
	Warnf(format string, args ...any)
	Warn(args ...any)
	Infof(format string, args ...any)
	Info(args ...any)
	Debugf(format string, args ...any)
	Debug(args ...any)
	Tracef(format string, args ...any)
	Trace(args ...any)
	Fatalf(format string, args ...any)
	Fatal(args ...any)
	WithFields(fields ...any) MessageLogger
	Nested(fields ...any) Logger
}

type MessageLogger interface {
	Logger
	Msgf(format string, args ...any)
	Msg(args ...any)
}

type LogLevel int

const (
	LevelTrace LogLevel = iota
	LevelDebug
	LevelInfo
	LevelWarn
	LevelError
	LevelFatal
)

const (
	ColorReset  = "\033[0m"
	ColorRed    = "\033[0;31m"
	ColorYellow = "\033[1;33m"
	ColorBlue   = "\033[0;34m"
)

func ParseLogLevel(level string) LogLevel {
	switch strings.ToLower(strings.TrimSpace(level)) {
	case "trace":
		return LevelTrace
	case "debug":
		return LevelDebug
	case "info":
		return LevelInfo
	case "warn", "warning":
		return LevelWarn
	case "error":
		return LevelError
	case "fatal", "critical":
		return LevelFatal
	default:
		return LevelInfo
	}
}

func (l LogLevel) String() string {
	switch l {
	case LevelTrace:
		return "TRACE"
	case LevelDebug:
		return "DEBUG"
	case LevelInfo:
		return "INFO"
	case LevelWarn:
		return "WARN"
	case LevelError:
		return "ERROR"
	case LevelFatal:
		return "FATAL"
	default:
		return "UNKNOWN"
	}
}

func (l LogLevel) getLevelColor() string {
	switch l {
	case LevelTrace, LevelDebug, LevelInfo:
		return ColorBlue
	case LevelWarn:
		return ColorYellow
	case LevelError, LevelFatal:
		return ColorRed
	default:
		return ColorReset
	}
}

func strip(s string) string {
	return strings.TrimSpace(s)
}

type ConstantinLogger struct {
	logLevel LogLevel
}

type messageLoggerWrapper struct {
	*ConstantinLogger
}

var customLog = log.New(os.Stderr, "", 0)

func New(level LogLevel) *ConstantinLogger {
	return &ConstantinLogger{logLevel: level}
}

func NewFromString(level string) *ConstantinLogger {
	return &ConstantinLogger{logLevel: ParseLogLevel(level)}
}

func (l *ConstantinLogger) SetLevel(level LogLevel) {
	l.logLevel = level
}

func (l *ConstantinLogger) GetLevel() LogLevel {
	return l.logLevel
}

func (l *ConstantinLogger) shouldLog(level LogLevel) bool {
	return level >= l.logLevel
}

func (l *ConstantinLogger) formatPrefix(level LogLevel) string {
	color := level.getLevelColor()
	return fmt.Sprintf("%s[%s] %s", color, level.String(), ColorReset)
}

func (l *ConstantinLogger) logf(level LogLevel, format string, args ...any) {
	if !l.shouldLog(level) {
		return
	}

	msg := strip(fmt.Sprintf(format, args...))
	customLog.Printf("%s%s", l.formatPrefix(level), msg)
}

func (l *ConstantinLogger) log(level LogLevel, args ...any) {
	if !l.shouldLog(level) {
		return
	}

	msg := strip(fmt.Sprint(args...))
	customLog.Println(l.formatPrefix(level) + msg)
}

func (l *ConstantinLogger) Errorf(format string, args ...any) { l.logf(LevelError, format, args...) }
func (l *ConstantinLogger) Error(args ...any)                 { l.log(LevelError, args...) }

func (l *ConstantinLogger) Warnf(format string, args ...any) { l.logf(LevelWarn, format, args...) }
func (l *ConstantinLogger) Warn(args ...any)                 { l.log(LevelWarn, args...) }

func (l *ConstantinLogger) Infof(format string, args ...any) { l.logf(LevelInfo, format, args...) }
func (l *ConstantinLogger) Info(args ...any)                 { l.log(LevelInfo, args...) }

func (l *ConstantinLogger) Debugf(format string, args ...any) { l.logf(LevelDebug, format, args...) }
func (l *ConstantinLogger) Debug(args ...any)                 { l.log(LevelDebug, args...) }

func (l *ConstantinLogger) Tracef(format string, args ...any) { l.logf(LevelTrace, format, args...) }
func (l *ConstantinLogger) Trace(args ...any)                 { l.log(LevelTrace, args...) }

func (l *ConstantinLogger) Fatalf(format string, args ...any) {
	msg := strip(fmt.Sprintf(format, args...))
	customLog.Fatalf("%s%s", l.formatPrefix(LevelFatal), msg)
}

func (l *ConstantinLogger) Fatal(args ...any) {
	msg := strip(fmt.Sprint(args...))
	customLog.Fatalln(l.formatPrefix(LevelFatal) + msg)
}

func (l *ConstantinLogger) WithFields(fields ...any) MessageLogger {
	return &messageLoggerWrapper{l}
}

func (l *ConstantinLogger) Nested(fields ...any) Logger {
	return &ConstantinLogger{
		logLevel: l.logLevel,
	}
}

func (m *messageLoggerWrapper) Errorf(format string, args ...any) {
	m.ConstantinLogger.Errorf(format, args...)
}
func (m *messageLoggerWrapper) Error(args ...any) { m.ConstantinLogger.Error(args...) }
func (m *messageLoggerWrapper) Warnf(format string, args ...any) {
	m.ConstantinLogger.Warnf(format, args...)
}
func (m *messageLoggerWrapper) Warn(args ...any) { m.ConstantinLogger.Warn(args...) }
func (m *messageLoggerWrapper) Infof(format string, args ...any) {
	m.ConstantinLogger.Infof(format, args...)
}
func (m *messageLoggerWrapper) Info(args ...any) { m.ConstantinLogger.Info(args...) }
func (m *messageLoggerWrapper) Debugf(format string, args ...any) {
	m.ConstantinLogger.Debugf(format, args...)
}
func (m *messageLoggerWrapper) Debug(args ...any) { m.ConstantinLogger.Debug(args...) }
func (m *messageLoggerWrapper) Tracef(format string, args ...any) {
	m.ConstantinLogger.Tracef(format, args...)
}
func (m *messageLoggerWrapper) Trace(args ...any) { m.ConstantinLogger.Trace(args...) }
func (m *messageLoggerWrapper) Msgf(format string, args ...any) {
	m.ConstantinLogger.Infof(format, args...)
}
func (m *messageLoggerWrapper) Msg(args ...any) { m.ConstantinLogger.Info(args...) }

func (m *messageLoggerWrapper) WithFields(fields ...any) MessageLogger {
	return m.ConstantinLogger.WithFields(fields...)
}
func (m *messageLoggerWrapper) Nested(fields ...any) Logger {
	return m.ConstantinLogger.Nested(fields...)
}
